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

#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "memory.h"

class debugger_memory_editor
{
	bool    m_open;
	bool    m_allow_edits;
	bool    m_reset_window;
	memory_high_read_type     m_show_ram;
	memory_high_read_bank     m_bank_num;
	int     m_columns;
	int     m_data_editing_address;
	bool    m_data_editing_take_focus;
	char    m_data_input[32];
	char    m_addr_input[32];

public:
	debugger_memory_editor();
	~debugger_memory_editor();
	void draw(const char* title, int mem_size, size_t base_display_addr = 0);
};


