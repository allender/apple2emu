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
#include <cstring>
#include <cassert>

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
static const uint32_t Max_breakpoints = 15;

static const uint32_t Debugger_status_line_length = 256;
static const uint32_t Debugger_disassembly_line_length = 256;
static char Debugger_status_line[Debugger_status_line_length];
static char Debugger_disassembly_line[Debugger_disassembly_line_length];

static bool Debugger_active = false;
static bool Debugger_stopped = false;
static bool Debugger_trace = false;
static FILE *Debugger_trace_fp = nullptr;

static breakpoint Debugger_breakpoints[Max_breakpoints];
static int Debugger_num_breakpoints;

// Variables for windows
static WINDOW *Debugger_memory_window;
static WINDOW *Debugger_register_window;
static WINDOW *Debugger_command_window;
static WINDOW *Debugger_disasm_window;
static WINDOW *Debugger_breakpoint_window;
static WINDOW *Debugger_status_window;
static uint32_t Debugger_memory_display_bytes = 16;
static uint32_t Debugger_memory_num_lines = 12;
static uint32_t Debugger_register_num_lines = 9;
static uint32_t Debugger_command_num_lines = 5;
static uint32_t Debugger_disasm_num_lines = 0;
static uint32_t Debugger_breakpoint_num_lines = 0;
static uint32_t Debugger_status_num_lines = 0;
static uint32_t Debugger_column_one_width = 80;
static uint32_t Debugger_column_two_width = 30;
static uint32_t Debugger_memory_display_address = 0;

static const char *addressing_format_string[] = {
	"",           // NO_MODE
	"",           // ACCUMULATOR_MODE
	"#$%02X",     // IMMEDIATE_MODE
	"",           // IMPLIED_MODE
	"$%02X",      // RELATIVE_MODE
	"$%04X",      // ABSOLUTE_MODE
	"$%02X",      // ZERO_PAGE_MODE
	"$(%04X)",    // INDIRECT_MODE
	"$%04X,X",    // X_INDEXED_MODE
	"$%04X,Y",    // Y_INDEXED_MODE
	"$%02X,X",    // ZERO_PAGES_INDEXED_MODE
	"$%02X,Y",    // ZERO_PAGED_INDEXED_MODE_Y
	"($%02X,X)",  // INDEXED_INDIRECT_MODE
	"$(%02X),Y",  // INDIRECT_INDEXED_MODE
};

// print out the status (PC, regs, etc)
static void debugger_get_short_status(cpu_6502 &cpu)
{
	uint8_t status = cpu.get_status();
	sprintf(Debugger_status_line, "%02x %02X %02X %04X %c%c%c%c%c%c%c%c", cpu.get_acc(), cpu.get_x(), cpu.get_y(), cpu.get_sp() + 0x100, 
		(status >> 7) & 1 ? 'N' : '.',
		(status >> 6) & 1 ? 'V' : '.',
		(status >> 5) & 1 ? 'R' : '.',
		(status >> 4) & 1 ? 'B' : '.',
		(status >> 3) & 1 ? 'D' : '.',
		(status >> 2) & 1 ? 'I' : '.',
		(status >> 1) & 1 ? 'Z' : '.',
		(status >> 0) & 1 ? 'C' : '.');
}

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
static uint8_t debugger_get_disassembly(cpu_6502 &cpu, memory &mem, uint32_t addr)
{
	char internal_buffer[Debugger_disassembly_line_length];
	Debugger_disassembly_line[0] = '\0';

	// get the opcode at the address and from there we have the mode
	cpu_6502::opcode_info *opcode = &cpu_6502::m_opcodes[mem[addr]];
	if (opcode->m_mnemonic == '    ') {
		// invalid opcode.  just print ?? and continue
		strcpy(Debugger_disassembly_line, "???");
	} else {
		sprintf(internal_buffer, "%04X:", addr);
		strcat(Debugger_disassembly_line, internal_buffer);

		for (uint8_t j = 0; j < 3; j++) {
			if (j < opcode->m_size) {
				sprintf(internal_buffer, "%02X ", mem[addr + j]);
			} else {
				sprintf(internal_buffer, "   ");
			}
			strcat(Debugger_disassembly_line, internal_buffer);
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
			uint32_t mem_value = -1;

			cpu_6502::addressing_mode mode = opcode->m_addressing_mode;
			if (mode == cpu_6502::addressing_mode::RELATIVE_MODE) {
				addressing_val = addr + mem[addr + 1] + 2;
				if (mem[addr + 1] & 0x80) {
					addressing_val -= 0x100;
				}
			}
			sprintf(internal_buffer, addressing_format_string[static_cast<uint8_t>(mode)], addressing_val);
			strcat(Debugger_disassembly_line, internal_buffer);

			switch(mode) {
			case cpu_6502::addressing_mode::ACCUMULATOR_MODE:
			case cpu_6502::addressing_mode::IMMEDIATE_MODE:
			case cpu_6502::addressing_mode::IMPLIED_MODE:
			case cpu_6502::addressing_mode::INDIRECT_MODE:
         case cpu_6502::addressing_mode::NO_MODE:
         case cpu_6502::addressing_mode::NUM_ADDRESSING_MODES:
				break;

			// relative mode is relative to current PC.  figure out if we need to
			// move forwards or backwards
			case cpu_6502::addressing_mode::RELATIVE_MODE:
				//if (addressing_val & 0x80) {
				//	mem_value = cpu.get_pc() - (addressing_val & 0x80);
				//} else {
				//	mem_value = cpu.get_pc() + addressing_val;
				//}
				break;

			case cpu_6502::addressing_mode::ABSOLUTE_MODE:
			case cpu_6502::addressing_mode::ZERO_PAGE_MODE:
				if (opcode->m_mnemonic != 'JSR ') {
					mem_value = mem[addressing_val];
					sprintf(internal_buffer, "\t%04X: 0x%02X", addressing_val, mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
					if (isprint(mem_value)) {
						sprintf(internal_buffer, " (%c)", mem_value);
						strcat(Debugger_disassembly_line, internal_buffer);
					}
				}
				break;

			// indexed and zero page indexed are the same
			case cpu_6502::addressing_mode::X_INDEXED_MODE:
			case cpu_6502::addressing_mode::ZERO_PAGE_INDEXED_MODE:
				addressing_val += cpu.get_x();
				mem_value = mem[addressing_val];
				sprintf(internal_buffer, "\t%04X: 0x%02X", addressing_val, mem_value);
				strcat(Debugger_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
				}
				break;

			// same as x-indexed mode
			case cpu_6502::addressing_mode::Y_INDEXED_MODE:
			case cpu_6502::addressing_mode::ZERO_PAGE_INDEXED_MODE_Y:
				addressing_val += cpu.get_y();
				mem_value = mem[addressing_val];
				sprintf(internal_buffer, "\t%04X: 0x%02X", addressing_val, mem_value);
				strcat(Debugger_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
				}
				break;

			case cpu_6502::addressing_mode::INDEXED_INDIRECT_MODE:
				//addressing_val += cpu.get_x();
				//addressing_val = (mem[addressing_val + 2] << 8) | mem[addressing_val + 1];
				//mem_value = mem[addressing_val];
				//sprintf(internal_buffer, "\t\t%04X: 0x%02X", addressing_val, mem_value);
				//strcat(Debugger_disassembly_line, internal_buffer);
				//if (isprint(mem_value)) {
				//	sprintf(internal_buffer, " (%c)", mem_value);
				//	strcat(Debugger_disassembly_line, internal_buffer);
				//}
				break;

			case cpu_6502::addressing_mode::INDIRECT_INDEXED_MODE:
				//addressing_val = (mem[addressing_val + 2] << 8) | mem[addressing_val + 1];
				//addressing_val += cpu.get_y();
				//mem_value = mem[addressing_val];
				//sprintf(internal_buffer, "\t\t%04X: 0x%02X", addressing_val, mem_value);
				//strcat(Debugger_disassembly_line, internal_buffer);
				//if (isprint(mem_value)) {
				//	sprintf(internal_buffer, " (%c)", mem_value);
				//	strcat(Debugger_disassembly_line, internal_buffer);
				//}
				break;
			}
		}
	}

	return opcode->m_size;
}

// disassemble at the given address
static void debugger_disassemble(cpu_6502 &cpu, memory &mem, uint32_t addr, int32_t num_lines = 20)
{
	for (auto i = 0; i < num_lines; i++) {
		uint8_t size = debugger_get_disassembly(cpu, mem, addr);
		printw("%s\n", Debugger_disassembly_line);
		addr += size;
	}
}

static void debugger_stop_trace()
{
	if (Debugger_trace_fp != nullptr) {
		fclose(Debugger_trace_fp);
		printw("Stopping trace\n");
	}
}

static void debugger_start_trace()
{
	// stop any trace currently running
	debugger_stop_trace();

	Debugger_trace_fp = fopen("trace.log", "wt");
	if (Debugger_trace_fp == nullptr) {
		printw("Unable to open trace file\n");
	}
	printw("Staring trace\n");
}

static void debugger_trace_line(memory& mem, cpu_6502& cpu)
{
	if (Debugger_trace_fp == nullptr) {
		return;
	}

	// print out the info the trace file
	debugger_get_short_status(cpu);
	debugger_get_disassembly(cpu, mem, cpu.get_pc());
	fprintf(Debugger_trace_fp, "%s  %s\n", Debugger_status_line, Debugger_disassembly_line);
}

static void debugger_display_memory(memory &mem)
{
	wclear(Debugger_memory_window);
	box(Debugger_memory_window, 0, 0);
	uint32_t addr = Debugger_memory_display_address;
	for (auto i = 0; i < Debugger_memory_num_lines-2; i++) {
		mvwprintw(Debugger_memory_window, i+1, 2, "%04X: ", addr);
		for (auto j = 0; j < Debugger_memory_display_bytes; j++) {
			wprintw(Debugger_memory_window, "%02x ", mem[addr+j]);
		}
		wprintw(Debugger_memory_window, "  ");

		for (auto j = 0; j < Debugger_memory_display_bytes; j++) {
			wprintw(Debugger_memory_window, "%c", isprint(mem[addr + j]) ? mem[addr + j] : '.');
		}
		addr += Debugger_memory_display_bytes;
	}
	wrefresh(Debugger_memory_window);
}

static void debugger_display_registers(cpu_6502 &cpu)
{
	uint32_t row = 1;
	wclear(Debugger_register_window);
	box(Debugger_register_window, 0, 0);
	mvwprintw(Debugger_register_window, row++, 2, "A  = $%02X", cpu.get_acc());
	mvwprintw(Debugger_register_window, row++, 2, "X  = $%02X", cpu.get_x());
	mvwprintw(Debugger_register_window, row++, 2, "Y  = $%02X", cpu.get_y());
	mvwprintw(Debugger_register_window, row++, 2, "PC = $%04X", cpu.get_pc());
	mvwprintw(Debugger_register_window, row++, 2, "SP = $%04X", cpu.get_sp() + 0x100);

	auto status = cpu.get_status();
	mvwprintw(Debugger_register_window, ++row, 2, "Status = %c%c%c%c%c%c%c%c", 
		(status >> 7) & 1 ? 'S' : 's',
		(status >> 6) & 1 ? 'V' : 'v',
		(status >> 5) & 1 ? '1' : '0',
		(status >> 4) & 1 ? 'B' : 'b',
		(status >> 3) & 1 ? 'D' : 'd',
		(status >> 2) & 1 ? 'I' : 'i',
		(status >> 1) & 1 ? 'Z' : 'z',
		(status >> 0) & 1 ? 'C' : 'c');
	wrefresh(Debugger_register_window);
}

// display the disassembly in the disassembly window
static void debugger_display_disasm(cpu_6502 &cpu, memory &mem)
{
	wclear(Debugger_disasm_window);
	box(Debugger_disasm_window, 0, 0);
	// for now, just disassm from the current pc
	auto addr = cpu.get_pc();
	auto row = 1;
	wattron(Debugger_disasm_window, A_REVERSE);
	for (auto i = 0; i < Debugger_disasm_num_lines - 2; i++) {
		auto size = debugger_get_disassembly(cpu, mem, addr);
		// see if there is breakpoint at given line and show that in red
		for (auto j = 0; j < Debugger_num_breakpoints; j++) {
			if (Debugger_breakpoints[j].m_addr == addr && Debugger_breakpoints[j].m_enabled) {
				wattron(Debugger_disasm_window, A_BOLD);
			}
		}
		mvwprintw(Debugger_disasm_window, row++, 2,"%s", Debugger_disassembly_line);
		wattroff(Debugger_disasm_window, A_REVERSE|A_BOLD);
		addr += size;
	}
	wrefresh(Debugger_disasm_window);
}

// display ll breakpoints
static void debugger_display_breakpoints()
{
	wclear(Debugger_breakpoint_window);
	box(Debugger_breakpoint_window, 0, 0);
	// for now, just disassm from the current pc
	for (auto i = 0; i < Debugger_num_breakpoints; i++) {
		wmove(Debugger_breakpoint_window, i + 1, 1);
		wprintw(Debugger_breakpoint_window, "%-3d", i);
		switch(Debugger_breakpoints[i].m_type) {
		case breakpoint_type::BREAKPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Bp");
			break;

		case breakpoint_type::RWATCHPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Rwp");
			break;

		case breakpoint_type::WWATCHPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Wwp");
			break;

      case breakpoint_type::INVALID:
         break;
		}
		
		wprintw(Debugger_breakpoint_window, "$%-6x", Debugger_breakpoints[i].m_addr);
		wprintw(Debugger_breakpoint_window, "%s", Debugger_breakpoints[i].m_enabled == true ? "1" : "0");

	}
	wrefresh(Debugger_breakpoint_window);
}

static void debugger_display_status(memory &mem)
{
	wclear(Debugger_status_window);
	box(Debugger_status_window, 0, 0);

	auto row = 1;
	wmove(Debugger_status_window, row++, 1);
	waddstr(Debugger_status_window, "$c050: ");
	if (Video_mode & VIDEO_MODE_TEXT) {
		waddstr(Debugger_status_window, "Graphics/");
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Text");
		wattroff(Debugger_status_window, A_REVERSE);
	} else {
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Graphics");
		wattroff(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "/Text");
	}
	wmove(Debugger_status_window, row++, 1);
	waddstr(Debugger_status_window, "$c052: ");
	if (Video_mode & VIDEO_MODE_MIXED) {
		waddstr(Debugger_status_window, "Not Mixed/");
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Mixed");
		wattroff(Debugger_status_window, A_REVERSE);
	} else {
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Not Mixed");
		wattroff(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "/Mixed");
	}
	wmove(Debugger_status_window, row++, 1);
	waddstr(Debugger_status_window, "$c054: ");
	if (Video_mode & VIDEO_MODE_PRIMARY) {
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Primary");
		wattroff(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "/Secondary");
	} else {
		waddstr(Debugger_status_window, "Primary/");
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Secondary");
		wattroff(Debugger_status_window, A_REVERSE);
	}
	wmove(Debugger_status_window, row++, 1);
	waddstr(Debugger_status_window, "$c056: ");
	if (Video_mode & VIDEO_MODE_HIRES) {
		waddstr(Debugger_status_window, "Lores/");
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Hires");
		wattroff(Debugger_status_window, A_REVERSE);
	} else {
		wattron(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "Lores");
		wattroff(Debugger_status_window, A_REVERSE);
		waddstr(Debugger_status_window, "/Hires");
	}

	wrefresh(Debugger_status_window);
}

// processes a command for the debugger.  Returns true when control
// should return back to CPU
static void debugger_process_commands(cpu_6502 &cpu, memory &mem)
{
	char input[Max_input];

	while (1) {

		// dump memory into memory window
		debugger_display_memory(mem);
		debugger_display_registers(cpu);
		debugger_display_disasm(cpu, mem);
		debugger_display_breakpoints();
		debugger_display_status(mem);

		debugger_get_status(cpu);
		debugger_get_disassembly(cpu, mem, cpu.get_pc());

		//wprintw(Debugger_command_window, "%s %s\n", Debugger_status_line, Debugger_disassembly_line);
		wprintw(Debugger_command_window, "> ");
		wgetstr(Debugger_command_window, input);

		// parse the debugger commands
		char *token = strtok(input, " ");
		if (token == nullptr) {
			//wprintw(Debugger_command_window, "\n");
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
			assert(bp_type != breakpoint_type::INVALID);

			if (Debugger_num_breakpoints < Max_breakpoints) {
				token = strtok(nullptr, " ");
				if (token != nullptr) {
					uint32_t address = strtol(token, nullptr, 16);
					if (address > 0xffff) { 
						printw("Address %x must be in range 0-0xffff\n", address);
					} else {
						Debugger_breakpoints[Debugger_num_breakpoints].m_type = bp_type;
						Debugger_breakpoints[Debugger_num_breakpoints].m_addr = address;
                  Debugger_breakpoints[Debugger_num_breakpoints].m_enabled = true;
                  Debugger_num_breakpoints++;
					}
				}
			}
			else {
				printw("Maximum number of breakpoints reached (%d)\n", Max_breakpoints);
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
			debugger_disassemble(cpu, mem, addr);
		}

		// view memory
		else if (!stricmp(token, "x") || !stricmp(token, "examine")) {
			token = strtok(nullptr, " ");
			if (token != nullptr) {
				Debugger_memory_display_address = strtol(token, nullptr, 16);
			}
		}

      // quit
      else if (!stricmp(token, "q") || !stricmp(token, "quit")) {
         debugger_shutdown();  // clears out curses changes to console
         exit(-1);
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

	box(Debugger_register_window, 0, 0);
}

void debugger_exit()
{
}

void debugger_shutdown()
{
   endwin();
}

void debugger_init()
{
	Debugger_active = false;
	Debugger_stopped = false;
	Debugger_num_breakpoints = 0;

	// initialize curses for the debugging window
	initscr();
	scrollok(stdscr, true);

   // set the disassembly window size after initting the screen so we know
   // how many lines and columns we have
   Debugger_disasm_num_lines = LINES - Debugger_memory_num_lines - Debugger_command_num_lines;
   Debugger_breakpoint_num_lines = (LINES - Debugger_register_num_lines) / 2;
   Debugger_status_num_lines = Debugger_breakpoint_num_lines;

	// create some windows for debugger use
	int row = 0;
	Debugger_memory_window = subwin(stdscr, Debugger_memory_num_lines, Debugger_column_one_width, row, 0);
	row += Debugger_memory_num_lines;
	Debugger_disasm_window = subwin(stdscr, Debugger_disasm_num_lines, Debugger_column_one_width, row, 0);
	row += Debugger_disasm_num_lines;
	Debugger_command_window = subwin(stdscr, Debugger_command_num_lines, Debugger_column_one_width, row, 0);
   scrollok(Debugger_command_window, true);

	row = 0;
	Debugger_register_window = subwin(stdscr, Debugger_register_num_lines, Debugger_column_two_width, row, 81);
	row += Debugger_register_num_lines;
	Debugger_breakpoint_window = subwin(stdscr, Debugger_breakpoint_num_lines, Debugger_column_two_width, row, 81);
	row += Debugger_breakpoint_num_lines;
	Debugger_status_window = subwin(stdscr, Debugger_status_num_lines, Debugger_column_two_width, row, 81);
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
				case breakpoint_type::WWATCHPOINT:
            case breakpoint_type::INVALID:
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
