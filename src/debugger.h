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

#pragma once

#include <vector>

enum class debugger_state {
	IDLE,
	WAITING_FOR_INPUT,
	SINGLE_STEP,
	STEP_OVER,
	SHOW_ALL,
};

enum class breakpoint_type {
	INVALID,
	BREAKPOINT,
	RWATCHPOINT,
	WWATCHPOINT,
	TEMPORARY,
};

struct breakpoint {
	breakpoint_type    m_type;
	uint16_t           m_addr;
	bool               m_enabled;
};

// commmands for the debugger - need access to these from the disassembler
extern const char *step_command_name;
extern const char *next_command_name;
extern const char *continue_command_name;
extern const char *break_command_name;


extern debugger_state Debugger_state;
extern bool Debugger_use_sym_tables;
extern std::vector<breakpoint> Debugger_breakpoints;

void debugger_enter();
void debugger_exit();
void debugger_init();
void debugger_shutdown();
bool debugger_process();
void debugger_render();
bool debugger_active();
void debugger_print_char_to_console(uint8_t c);
void debugger_reset_windows();

