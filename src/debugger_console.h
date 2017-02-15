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
#include <vector>
#include <map>
#include <functional>

#include "imgui.h"

class debugger_console
{
	static const int max_line_size = 1024;

	struct console_command {
		std::string                 m_help;
		std::function<void(char *)> m_func;
	};

	char                                   m_input_buf[max_line_size];
	std::vector<char*>                     m_items;
	bool                                   m_scroll_to_bottom;
	std::vector<char*>                     m_history;
	int                                    m_history_pos;    // -1: new line, 0..History.Size-1 browsing history.
	std::map<std::string, console_command> m_commands;

public:
	debugger_console();
	~debugger_console();
	int text_edit_callback(ImGuiTextEditCallbackData* data);
	void draw(const char* title, bool* p_open);
	void add_log(const char* fmt, ...) IM_PRINTFARGS(2);
	void add_command(const char *command, const char *help, std::function<void(char*)>);
	void execute_command(const char *command = nullptr);

private:
	void clear_log();
	int text_edit_callback_stub(ImGuiTextEditCallbackData* data);

};

