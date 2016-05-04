/*

MIT License

Copyright (c) 2016 Mark Allender


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <array>

#include "debugger/debugger.h"
#include "6502/video.h"
#include "6502/curses.h"
#include "panel.h"

enum class breakpoint_type {
	INVALID,
	BREAKPOINT,
	RWATCHPOINT,
	WWATCHPOINT,
};

struct breakpoint {
	breakpoint_type    m_type;
	uint16_t           m_addr;
	bool               m_enabled;
};

static const uint32_t Max_input = 256;
static const uint32_t Max_breakpoints = 256;

static const uint32_t Debugger_status_line_length = 256;
static const uint32_t Debugger_disassembly_line_length = 256;
static char Debugger_status_line[Debugger_status_line_length];
static char Debugger_disassembly_line[Debugger_disassembly_line_length];

static bool Debugger_active = false;
static bool Debugger_stopped = false;
static bool Debugger_next_statement = false;
static bool Debugger_trace = false;
static FILE *Debugger_trace_fp = nullptr;

static breakpoint Debugger_breakpoints[Max_breakpoints];
static int Debugger_num_breakpoints;

static const char *addressing_format_string[] = {
	"",           // NO_MODE
	"",           // ACCUMULATOR_MODE
	"#$%02X",     // IMMEDIATE_MODE
	"",           // IMPLIED_MODE
	"$%02X",      // RELATIVE_MODE
	"$%04X",      // ABSOLUTE_MODE
	"$%02X",      // ZERO_PAGE_MODE
	"($%04X)",    // INDIRECT_MODE
	"$%04X,X",    // X_INDEXED_MODE
	"$%04X,Y",    // Y_INDEXED_MODE
	"$%02X,X",    // ZERO_PAGES_INDEXED_MODE
	"$%02X,Y",    // ZERO_PAGED_INDEXED_MODE_Y
	"($%02X,X)",  // INDEXED_INDIRECT_MODE
	"($%02X),Y",  // INDIRECT_INDEXED_MODE
};

// print out the status (PC, regs, etc)
static void debugger_get_status(cpu_6502 &cpu)
{
	uint8_t status = cpu.get_status();
	sprintf(Debugger_status_line, "%04X   A=%02x   X=%02X   Y=%02X  Status = %c%c%c%c%c%c%c%c", cpu.get_pc(), cpu.get_acc(), cpu.get_x(), cpu.get_y(),
		(status >> 7) & 1 ? 'S' : 's',
		(status >> 6) & 1 ? 'V' : 'v',
		(status >> 5) & 1 ? '1' : '0',
		(status >> 4) & 1 ? 'B' : 'b',
		(status >> 3) & 1 ? 'D' : 'd',
		(status >> 2) & 1 ? 'I' : 'i',
		(status >> 1) & 1 ? 'Z' : 'z',
		(status >> 0) & 1 ? 'C' : 'c');
}

// disassemble the given address into the buffer
static uint8_t debugger_get_disassembly(memory &mem, uint32_t addr, bool include_hex = false)
{
	char internal_buffer[Debugger_disassembly_line_length];
	Debugger_disassembly_line[0] = '\0';

	// get the opcode at the address and from there we have the mode
	cpu_6502::opcode_info *opcode = &cpu_6502::m_opcodes[mem[addr]];
	if (opcode->m_mnemonic == '    ') {
		// invalid opcode.  just print ?? and continue
		strcpy(Debugger_disassembly_line, "???");
	} else {
		if (include_hex == true) {
			for (uint8_t j = 0; j < 3; j++) {
				if (j < opcode->m_size) {
					sprintf(internal_buffer, "%02X ", mem[addr + j]);
				} else {
					sprintf(internal_buffer, "   ");
				}
				strcat(Debugger_disassembly_line, internal_buffer);
			}
		}

		// print opcode
		sprintf(internal_buffer, "   %c%c%c ", opcode->m_mnemonic >> 24,
			opcode->m_mnemonic >> 16,
			opcode->m_mnemonic >> 8);

		strcat(Debugger_disassembly_line, internal_buffer);

		uint16_t addressing_val;
		if (opcode->m_size == 2) {
			addressing_val = mem[addr + 1];
		} else if (opcode->m_size == 3) {
			addressing_val = (mem[addr + 2] << 8) | mem[addr + 1];
		}
		if (opcode->m_size > 1) {
			sprintf(internal_buffer, addressing_format_string[static_cast<uint8_t>(opcode->m_addressing_mode)], addressing_val);
			strcat(Debugger_disassembly_line, internal_buffer);
		}
	}

	return opcode->m_size;
}

// disassemble at the given address
static void debugger_disassemble(memory &mem, uint32_t addr, int32_t num_lines = 20)
{
	for (auto i = 0; i < num_lines; i++) {
		uint8_t size = debugger_get_disassembly(mem, addr, true);
		printw("%s\n", Debugger_disassembly_line);
		addr += size;
	}
}

static void debugger_stop_trace()
{
	if (Debugger_trace_fp != nullptr) {
		fclose(Debugger_trace_fp);
	}
}

static void debugger_start_trace()
{
	// stop any trace currently running
	debugger_stop_trace();

	Debugger_trace_fp = fopen("trace.log", "wt");
	if (Debugger_trace_fp == nullptr) {
		printf("Unable to open trace file\n");
	}
}

static void debugger_trace_line(memory& mem, cpu_6502& cpu)
{
	if (Debugger_trace_fp == nullptr) {
		return;
	}

	// print out the info the trace file
	uint16_t addr = cpu.get_pc();
	debugger_get_status(cpu);
	debugger_get_disassembly(mem, cpu.get_pc(), true);
	fprintf(Debugger_trace_fp, "%s  %s\n", Debugger_status_line, Debugger_disassembly_line);
}

// processes a command for the debugger.  Returns true when control
// should return back to CPU
static void debugger_process_commands(cpu_6502 &cpu, memory &mem)
{
	char input[Max_input];

	while (1) {
		debugger_get_status(cpu);
		debugger_get_disassembly(mem, cpu.get_pc());
		printw("%s %s\n", Debugger_status_line, Debugger_disassembly_line);
		printw("> ");
		getstr(input);

		// parse the debugger commands
		char *token = strtok(input, " ");
		if (token == nullptr) {
			printw("\n");
			continue;
		}

		// step to next statement
		if (!stricmp(token, "s") || !stricmp(token, "step")) {
			break;

		// continue
		} else if (!stricmp(token, "c") || !stricmp(token, "cont")) {
			Debugger_stopped = false;
			break;
		

		// create a breakpoint
		} else if (!stricmp(token, "b") || !stricmp(token, "break") ||
				!stricmp(token, "rw") || !stricmp(token, "rwatch") ||
				!stricmp(token, "ww") || !stricmp(token, "wwatch") ) {
			// breakpoint at an address
			breakpoint_type bp_type = breakpoint_type::INVALID;

			if (token[0] == 'b') {
				bp_type = breakpoint_type::BREAKPOINT;
			} else if (token[0] == 'r') {
				bp_type = breakpoint_type::RWATCHPOINT;
			} else if (token[0] == 'w') {
				bp_type = breakpoint_type::WWATCHPOINT;
			}
			_ASSERT(bp_type != breakpoint_type::INVALID);

			if (Debugger_num_breakpoints < Max_breakpoints) {
				token = strtok(nullptr, " ");
				uint32_t address = strtol(token, nullptr, 16);
				if (address > 0xffff) { 
					printw("Address %x must be in range 0-0xffff\n", address);
				} else {
					Debugger_breakpoints[Debugger_num_breakpoints].m_type = bp_type;
					Debugger_breakpoints[Debugger_num_breakpoints].m_addr = address; Debugger_breakpoints[Debugger_num_breakpoints].m_enabled = true; Debugger_num_breakpoints++;
				}
			}
			else {
				printw("Maximum number of breakpoints reached (%d)\n", Max_breakpoints);
			}

		// list info on breakpoints or watchpoints
		} else if (!stricmp(token, "info")) {
			token = strtok(nullptr, " ");
			if (token == nullptr) {
				printw("\n");
			} else if (!stricmp(token, "break") || !stricmp(token, "watch")) {
				printw("%-5s%-20s%-10s%s\n", "Num", "Type", "Address", "Enabled");
				for (auto i = 0; i < Debugger_num_breakpoints; i++) {
					printw("%-5d", i);
					switch(Debugger_breakpoints[i].m_type) {
					case breakpoint_type::BREAKPOINT:
						printw("%-20s", "Breakpoint");
						break;

					case breakpoint_type::RWATCHPOINT:
						printw("%-20s", "Read watchpoint");
						break;

					case breakpoint_type::WWATCHPOINT:
						printw("%-20s", "Write watchpoint");
						break;
					}
					
					printw("0x%-8x", Debugger_breakpoints[i].m_addr);
					printw("%s\n", Debugger_breakpoints[i].m_enabled == true ? "true" : "false");

				}
			}
		}

		// enable and disable breakpoints
		else if (!stricmp(token, "enable") || !stricmp(token, "disable")) {
			bool enable = !stricmp(token, "enable");
			token = strtok(nullptr, " ");
			if (token == nullptr) {
				// enable all breakpoints
				for (auto i = 0; i < Debugger_num_breakpoints; i++) {
					Debugger_breakpoints[i].m_enabled = enable;
				}
			} else {
				int i = atoi(token);
				if (i >= 0 && i < Debugger_num_breakpoints) {
					Debugger_breakpoints[i].m_enabled = enable;
				}
			}
		}

		// delete breakpoints
		else if (!stricmp(token, "del")) {
			token = strtok(nullptr, " ");
			if (token == nullptr) {
				// delete all  breakpoints
				Debugger_num_breakpoints = 0;
			} else {
				int i = atoi(token);
				if (i >= 0 && i < Debugger_num_breakpoints) {
					for (auto j = i + 1; j < Debugger_num_breakpoints; j++) {
						Debugger_breakpoints[j - 1] = Debugger_breakpoints[j];
					}
					Debugger_num_breakpoints--;
				}
			}
		}

		// disassemble.  With no address, use current PC
		// if address given, then disassemble at that adddress
		else if (!stricmp(token, "dis")) {
			uint32_t addr;

			token = strtok(nullptr, " ");
			if (token == nullptr) {
				addr = cpu.get_pc();
			} else {
				addr = strtol(token, nullptr, 16);
			}
			debugger_disassemble(mem, addr);
		}

		// view memory
		else if (!stricmp(token, "v") || !stricmp(token, "view")) {
		}

		// stop/start trace
		else if (!stricmp(token, "trace")) {

			Debugger_trace = !Debugger_trace;
			if (Debugger_trace == true) {
				debugger_start_trace();
			} else {
				debugger_stop_trace();
			}

		}
	}
}

void debugger_enter()
{
	Debugger_active = true;
	Debugger_stopped = true;
#if !defined(USE_SDL)
	set_raw(false);
	printw("\n");
#endif
}

void debugger_exit()
{
#if !defined(USE_SDL)
	set_raw(true);
#endif
}

void debugger_init()
{
	Debugger_active = false;
	Debugger_stopped = false;
	Debugger_num_breakpoints = 0;
}

void debugger_process(cpu_6502 &cpu, memory &mem)
{
	// check on breakpoints
	if (Debugger_stopped == false) {
		for (auto i = 0; i < Debugger_num_breakpoints; i++) {
			if (Debugger_breakpoints[i].m_enabled == true) {
				switch (Debugger_breakpoints[i].m_type) {
				case breakpoint_type::BREAKPOINT:
					if (cpu.get_pc() == Debugger_breakpoints[i].m_addr) {
						debugger_enter();
					}
					break;
				case breakpoint_type::RWATCHPOINT:
					break;
				case breakpoint_type::WWATCHPOINT:
					break;
				}
			}
		}
	}
	// if we are stopped, process any commands here in tight loop
	if (Debugger_stopped) {
		debugger_process_commands(cpu, mem);
		if (Debugger_stopped == false) {
			debugger_exit();
		}
	}

	if (Debugger_trace_fp != nullptr) {
		debugger_trace_line(mem, cpu);
	}

}