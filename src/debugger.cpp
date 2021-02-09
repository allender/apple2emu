/*

   MIT License

   Copyright (c) 2016-2017 Mark Allender


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
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "debugger.h"
#include "memory.h"
#include "video.h"
#include "imgui.h"
#include "debugger_console.h"
#include "debugger_memory.h"
#include "debugger_disasm.h"

debugger_state Debugger_state = debugger_state::IDLE;

std::vector<breakpoint> Debugger_breakpoints;

static const uint32_t Debugger_status_line_length = 256;
static char Debugger_status_line[Debugger_status_line_length];

static FILE *Debugger_trace_fp = nullptr;

// definitions for windows
static debugger_console Debugger_console;
static debugger_memory_editor Debugger_memory_editor;
static debugger_disasm Debugger_disasm;
static bool Reset_windows = false;

const char *step_command_name = "step";
const char *next_command_name = "next";
const char *continue_command_name = "continue";
const char *break_command_name = "break";

// lambdas for console commands for debugger
auto step_command = [](char *) { Debugger_state = debugger_state::SINGLE_STEP; };
auto next_command = [](char *) {
	// look at current opcode. If jsr, then get next opcode
	// after jsr, store, and go into step over mode
	cpu_6502::opcode_info *opcode = cpu.get_opcode(memory_read(cpu.get_pc()));
	if (opcode->m_mnemonic == 'JSR ') {
		breakpoint b;

		b.m_type = breakpoint_type::TEMPORARY;
		b.m_addr = cpu.get_pc() + opcode->m_size;
		b.m_enabled = true;
		Debugger_breakpoints.push_back(b);
		Debugger_state = debugger_state::STEP_OVER;
	} else {
		Debugger_state = debugger_state::SINGLE_STEP;
	}

};
auto continue_command = [](char *) { Debugger_state = debugger_state::SHOW_ALL; };
auto stop_command = [](char *) { Debugger_state = debugger_state::SINGLE_STEP; };
auto exit_command = [](char *) { Debugger_state = debugger_state::IDLE; };

auto quit_command = [](char *) {
	exit(-1);
};

auto break_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		uint16_t address = (uint16_t)strtol(token, nullptr, 16);
		breakpoint b;

		// remove this breakpoint if it's already active
		auto remove_it = std::remove_if(Debugger_breakpoints.begin(), Debugger_breakpoints.end(),
			[address](const breakpoint &bp) {
			return ((address == bp.m_addr) && (bp.m_type == breakpoint_type::BREAKPOINT));
		});

		if (remove_it != Debugger_breakpoints.end()) {
			Debugger_breakpoints.erase(remove_it);
		} else {
			b.m_type = breakpoint_type::BREAKPOINT;
			b.m_addr = address;
			b.m_enabled = true;
			Debugger_breakpoints.push_back(b);
		}
	}
};

auto enable_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints[val].m_enabled = true;
			Debugger_console.add_log("Breakpoint %lu enabled.", val);
		} else {
			Debugger_console.add_log("%lu is not a valid index into breakpoint list.", val);
		}
	}
};

auto disable_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints[val].m_enabled = false;
			Debugger_console.add_log("Breakpoint %lu disabled.", val);
		} else {
			Debugger_console.add_log("%lu is not a valid index into breakpoint list.", val);
		}
	}
};

auto list_command = [](char *) {
	int index = 0;
	if (Debugger_breakpoints.size() == 0) {
		Debugger_console.add_log("No breakpoints defined.");
	} else {
		for (auto bp : Debugger_breakpoints) {
			if (bp.m_enabled) {
				Debugger_console.add_log("%d - 0x%04x enabled", index++, bp.m_addr);
			} else {
				Debugger_console.add_log("%d - 0x%04x disabled", index++, bp.m_addr);
			}
		}
	}
};

auto delete_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints.erase(Debugger_breakpoints.begin() + val);
			Debugger_console.add_log("Breakpoint %lu deleted.", val);
		} else {
			Debugger_console.add_log("%lu not a valid index into breakpoint list.", val);
		}
	}
};

auto trace_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (Debugger_trace_fp == nullptr) {
		const char *f = "trace.log";

		if (token != nullptr) {
			f = token;
		}

		Debugger_trace_fp = fopen(f, "wt");
		Debugger_console.add_log("Opening trace output to %s", f);
	} else {
		if (Debugger_trace_fp != nullptr) {
			fclose(Debugger_trace_fp);
			Debugger_trace_fp = nullptr;
			Debugger_console.add_log("Stopping trace output");
		}
	}
};

// print out the status (PC, regs, etc)
static void debugger_get_short_status()
{
	uint8_t status = cpu.get_status();
	sprintf(Debugger_status_line, "%02X %02X %02X %04X %c%c%c%c%c%c%c%c",
		cpu.get_acc(), cpu.get_x(), cpu.get_y(), cpu.get_sp() + 0x100,
		(status >> 7) & 1 ? 'N' : '.',
		(status >> 6) & 1 ? 'V' : '.',
		(status >> 5) & 1 ? 'R' : '.',
		(status >> 4) & 1 ? 'B' : '.',
		(status >> 3) & 1 ? 'D' : '.',
		(status >> 2) & 1 ? 'I' : '.',
		(status >> 1) & 1 ? 'Z' : '.',
		(status >> 0) & 1 ? 'C' : '.');
}

static void debugger_trace_line()
{
	if (Debugger_trace_fp == nullptr) {
		return;
	}

	// print out the info the trace file
	debugger_get_short_status();
	Debugger_disasm.get_disassembly(cpu.get_pc());
	fprintf(Debugger_trace_fp, "%08X %s  %s\n", Total_cycles, Debugger_status_line, Debugger_disasm.get_disassembly_line());
}

// display the disassembly in the disassembly window
void debugger_enter()
{
	// might need to go into debugging state if we started emulator
	// from debugger
	if (Emulator_state == emulator_state::SPLASH_SCREEN) {
		Emulator_state = emulator_state::EMULATOR_STARTED;
	}
	Debugger_state = debugger_state::WAITING_FOR_INPUT;
	Debugger_disasm.set_break_addr(cpu.get_pc());
}

void debugger_exit()
{
	Debugger_state = debugger_state::IDLE;
}

void debugger_shutdown()
{
}

void debugger_init()
{
	Debugger_state = debugger_state::IDLE;
	Debugger_breakpoints.clear();
	Debugger_disasm.attach_console(&Debugger_console);

	// add in the debugger commands
	Debugger_console.add_command(step_command_name, "Single step assembly", step_command);
	Debugger_console.add_command(next_command_name,
			"Next line in current stack frame", next_command);
	Debugger_console.add_command(continue_command_name,
		"Continue execution in debugger", continue_command);
	Debugger_console.add_command("stop",
		"Stop execution when in debugger", stop_command);
	Debugger_console.add_command("exit", "Exit debugger", exit_command);
	Debugger_console.add_command("quit", "Quit emulator", quit_command);
	Debugger_console.add_command("break",
		"Create breakpoint (break <addr>)", break_command);
	Debugger_console.add_command("enable",
		"Enable breakpoint (enable <bp#>)", enable_command);
	Debugger_console.add_command("disable",
		"Disable breakpoint (disable <bp#>)", disable_command);
	Debugger_console.add_command("list",
		"List breakpoints", list_command);
	Debugger_console.add_command("delete",
		"Delete breakpoint (delete bp#)", delete_command);
	Debugger_console.add_command("trace",
		"Toggle trace to file.  (trace <filename> - filename"
		" is optional and if not specified, output will be to 'trace.log'",
		trace_command);

}

bool debugger_process()
{
	bool continue_execution = true;
	static breakpoint *active_breakpoint  = nullptr;
	uint16_t pc = cpu.get_pc();

	// check on breakpoints
	if (Debugger_state == debugger_state::IDLE ||
		Debugger_state == debugger_state::SHOW_ALL ||
		Debugger_state == debugger_state::STEP_OVER) {
		for (size_t i = 0; i < Debugger_breakpoints.size(); i++) {
			if (Debugger_breakpoints[i].m_enabled == true &&
				active_breakpoint != &Debugger_breakpoints[i]) {
				switch (Debugger_breakpoints[i].m_type) {
				case breakpoint_type::BREAKPOINT:
				case breakpoint_type::TEMPORARY:
					if (pc == Debugger_breakpoints[i].m_addr) {
						debugger_enter();
						continue_execution = false;
						active_breakpoint = &Debugger_breakpoints[i];

						if (Debugger_breakpoints[i].m_type != breakpoint_type::TEMPORARY) {
							const char *addr_sym = Debugger_disasm.find_symbol(active_breakpoint->m_addr);
							if (addr_sym != nullptr) {
								Debugger_console.add_log("Stopped at breakpoint: %04x (%s)\n", active_breakpoint->m_addr, addr_sym);
							} else {
								Debugger_console.add_log("Stopped at breakpoint: %04x\n", active_breakpoint->m_addr);
							}
						}
					}
					break;
				case breakpoint_type::RWATCHPOINT:
				case breakpoint_type::WWATCHPOINT:
				case breakpoint_type::INVALID:
					break;
				}
			}
			// no need to keep processing breakpoints if we have
			// broken somewhere.  Remove temporary breakpoints
			if (continue_execution == false) {
				if (Debugger_breakpoints[i].m_type == breakpoint_type::TEMPORARY) {
					Debugger_breakpoints.erase(Debugger_breakpoints.begin() + i);
				}
				break;
			}
		}
	}

	else if (Debugger_state == debugger_state::WAITING_FOR_INPUT) {
		continue_execution = false;
	}

	// when single stepping, just go to input on the next opcode
	else if (Debugger_state == debugger_state::SINGLE_STEP) {
		Debugger_state = debugger_state::WAITING_FOR_INPUT;
	}

	// when stepping over or continuing, we need to move off of the
	// current line if we are sitting on the active breakpoint
	// and haven't done anything else
	if ((Debugger_state == debugger_state::STEP_OVER ||
		Debugger_state == debugger_state::SHOW_ALL) &&
		active_breakpoint != nullptr) {
		active_breakpoint = nullptr;
		continue_execution = true;
	}

	if (continue_execution == true && Debugger_trace_fp != nullptr) {
		debugger_trace_line();
	}


	return continue_execution;
}

bool debugger_active()
{
	return Debugger_state != debugger_state::IDLE;
}

void debugger_render()
{
	Debugger_memory_editor.draw("Memory", 0x10000, 0);
	Debugger_disasm.draw("Disassembly", cpu.get_pc());
	Debugger_console.draw("Console", nullptr);

	Reset_windows = false;
}

void debugger_print_char_to_console(uint8_t c)
{
	Debugger_console.add_log("%c", c);
}

void debugger_reset_windows()
{
	Debugger_memory_editor.reset();
	Debugger_disasm.reset();
	Debugger_console.reset();
	Reset_windows = true;       // ugh should make the other windows classes probably
}

void debugger_load_symbol_table(const std::string &filename)
{
	Debugger_disasm.add_symtable(filename);
}

void debugger_unload_symbol_table(const std::string &filename)
{
	Debugger_disasm.remove_symtable(filename);
}
