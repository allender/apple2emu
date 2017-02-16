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

#include <array>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "debugger_console.h"
#include "imgui.h"

class debugger_disasm
{
	static const uint32_t m_disassembly_line_length = 256;

private:

	// definitions for symbol tables
	typedef std::map<uint16_t, const char *> symtable_map;
	std::vector<symtable_map> m_symbol_tables;

    uint16_t m_break_addr;
    uint16_t m_current_addr;
	debugger_console *m_console;


	static const char *m_addressing_format_string[];
	char m_disassembly_line[m_disassembly_line_length];
	void add_symtable(const char *filename);
	const char *find_symbol(uint16_t addr);

public:
	debugger_disasm();
	~debugger_disasm();
	uint8_t get_disassembly(uint16_t addr);
	void draw(const char* title, uint16_t addr);
    void set_break_addr(uint16_t addr);
	char *get_disassembly_line() { return m_disassembly_line; }
	void attach_console(debugger_console *console);
};

