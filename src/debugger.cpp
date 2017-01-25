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

#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "debugger.h"
#include "memory.h"
#include "video.h"
#include "imgui.h"

enum class breakpoint_type {
	INVALID,
	BREAKPOINT,
	RWATCHPOINT,
	WWATCHPOINT,
};

enum class debugger_state {
	IDLE,
	WAITING_FOR_INPUT,
	SINGLE_STEP,
	SHOW_ALL,
};

struct breakpoint {
	breakpoint_type    m_type;
	uint16_t           m_addr;
	bool               m_enabled;
};

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

static const uint32_t Max_input = 256;

static const uint32_t Debugger_status_line_length = 256;
static const uint32_t Debugger_disassembly_line_length = 256;
static char Debugger_input[Max_input];
static char Debugger_status_line[Debugger_status_line_length];
static char Debugger_disassembly_line[Debugger_disassembly_line_length];

static debugger_state Debugger_state = debugger_state::IDLE;
static FILE *Debugger_trace_fp = nullptr;

static std::vector<breakpoint> Debugger_breakpoints;

//static ImGuiWindowFlags default_window_flags = ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
static ImGuiWindowFlags default_window_flags = ImGuiWindowFlags_ShowBorders;

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
static void debugger_get_short_status()
{
	uint8_t status = cpu.get_status();
	sprintf(Debugger_status_line, "%02x %02X %02X %04X %c%c%c%c%c%c%c%c",
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

// print out the status (PC, regs, etc)
static void debugger_get_status()
{
	uint8_t status = cpu.get_status();
	sprintf(Debugger_status_line,
		"%04X   A=%02x   X=%02X   Y=%02X  Status = %c%c%c%c%c%c%c%c",
		cpu.get_pc(), cpu.get_acc(), cpu.get_x(), cpu.get_y(),
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
static uint8_t debugger_get_disassembly(uint16_t addr)
{
	char internal_buffer[Debugger_disassembly_line_length];
	Debugger_disassembly_line[0] = '\0';

	// get the opcode at the address and from there we have the mode
	cpu_6502::opcode_info *opcode = &cpu_6502::m_opcodes[memory_read(addr)];
	if (opcode->m_mnemonic == '    ') {
		// invalid opcode.  just print ?? and continue
		strcpy(Debugger_disassembly_line, "???");
	}
	else {
		sprintf(internal_buffer, "%04X:", addr);
		strcat(Debugger_disassembly_line, internal_buffer);

		for (uint8_t j = 0; j < 3; j++) {
			if (j < opcode->m_size) {
				sprintf(internal_buffer, "%02X ", memory_read(addr + j));
			}
			else {
				sprintf(internal_buffer, "   ");
			}
			strcat(Debugger_disassembly_line, internal_buffer);
		}

		// print opcode
		sprintf(internal_buffer, "   %c%c%c ", opcode->m_mnemonic >> 24,
			opcode->m_mnemonic >> 16,
			opcode->m_mnemonic >> 8);

		strcat(Debugger_disassembly_line, internal_buffer);

		uint16_t addressing_val = 0;
		if (opcode->m_size == 2) {
			addressing_val = memory_read(addr + 1);
		}
		else if (opcode->m_size == 3) {
			addressing_val = (memory_read(addr + 2) << 8) | memory_read(addr + 1);
		}
		if (opcode->m_size > 1) {
			uint32_t mem_value = 0xffffffff;

			cpu_6502::addr_mode mode = opcode->m_addr_mode;
			if (mode == cpu_6502::addr_mode::RELATIVE_MODE) {
				addressing_val = addr + memory_read(addr + 1) + 2;
				if (memory_read(addr + 1) & 0x80) {
					addressing_val -= 0x100;
				}
			}
			sprintf(internal_buffer, addressing_format_string[static_cast<uint8_t>(mode)], addressing_val);
			strcat(Debugger_disassembly_line, internal_buffer);

			switch (mode) {
			case cpu_6502::addr_mode::ACCUMULATOR_MODE:
			case cpu_6502::addr_mode::IMMEDIATE_MODE:
			case cpu_6502::addr_mode::IMPLIED_MODE:
			case cpu_6502::addr_mode::INDIRECT_MODE:
			case cpu_6502::addr_mode::NO_MODE:
			case cpu_6502::addr_mode::NUM_ADDRESSING_MODES:
				break;

				// relative mode is relative to current PC.  figure out if we need to
				// move forwards or backwards
			case cpu_6502::addr_mode::RELATIVE_MODE:
				//if (addressing_val & 0x80) {
				//	mem_value = cpu.get_pc() - (addressing_val & 0x80);
				//} else {
				//	mem_value = cpu.get_pc() + addressing_val;
				//}
				break;

			case cpu_6502::addr_mode::ABSOLUTE_MODE:
			case cpu_6502::addr_mode::ZERO_PAGE_MODE:
				if (opcode->m_mnemonic != 'JSR ') {
					mem_value = memory_read(addressing_val);
					sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
					if (isprint(mem_value)) {
						sprintf(internal_buffer, " (%c)", mem_value);
						strcat(Debugger_disassembly_line, internal_buffer);
					}
				}
				break;

				// indexed and zero page indexed are the same
			case cpu_6502::addr_mode::X_INDEXED_MODE:
			case cpu_6502::addr_mode::ZP_INDEXED_MODE:
				addressing_val += cpu.get_x();
				mem_value = memory_read(addressing_val);
				sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
				strcat(Debugger_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
				}
				break;

				// same as x-indexed mode
			case cpu_6502::addr_mode::Y_INDEXED_MODE:
			case cpu_6502::addr_mode::ZP_INDEXED_MODE_Y:
				addressing_val += cpu.get_y();
				mem_value = memory_read(addressing_val);
				sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
				strcat(Debugger_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(Debugger_disassembly_line, internal_buffer);
				}
				break;

			case cpu_6502::addr_mode::INDEXED_INDIRECT_MODE:
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

			case cpu_6502::addr_mode::INDIRECT_INDEXED_MODE:
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

#if 0
// disassemble at the given address
static void debugger_disassemble(uint16_t addr, int32_t num_lines = 20)
{
	for (auto i = 0; i < num_lines; i++) {
		uint8_t size = debugger_get_disassembly(addr);
		addr += size;
	}
}
#endif

static void debugger_stop_trace()
{
	if (Debugger_trace_fp != nullptr) {
		fclose(Debugger_trace_fp);
		Debugger_trace_fp = nullptr;
	}
}

static void debugger_start_trace(char *filename)
{
	const char *f = "trace.log";

	if (filename != nullptr) {
		f = filename;
	}

	// stop any trace currently running
	debugger_stop_trace();

	Debugger_trace_fp = fopen(f, "wt");
}

static void debugger_trace_line()
{
	if (Debugger_trace_fp == nullptr) {
		return;
	}

	// print out the info the trace file
	debugger_get_short_status();
	debugger_get_disassembly(cpu.get_pc());
	fprintf(Debugger_trace_fp, "%s  %s\n", Debugger_status_line, Debugger_disassembly_line);
}

struct DebuggerConsole
{
	struct console_command {
		std::string           m_help;
		std::function<void()> m_func;
	};

	char                                   m_input_buf[256];
	ImVector<char*>                        m_items;
	bool                                   m_scroll_to_bottom;
	ImVector<char*>                        m_history;
	int                                    m_history_pos;    // -1: new line, 0..History.Size-1 browsing history.
	std::map<std::string, console_command> m_commands;


	DebuggerConsole()
	{
		clear_log();
		memset(m_input_buf, 0, sizeof(m_input_buf));
		m_history_pos = -1;
		m_commands["help"] = { "Show help ",
			[this]() {
						for (auto const &cmd : m_commands) {
							AddLog("%s - %s", cmd.first.c_str(), cmd.second.m_help.c_str());
						}
		} };
		m_commands["step"] = { "Single step assembly ",
			[]() { Debugger_state = debugger_state::SINGLE_STEP; } };
		m_commands["continue"] = { "Continue exectution in debugger",
			[]() { Debugger_state = debugger_state::SHOW_ALL; } };
		m_commands["stop"] = { "Stop execution when in debugger",
			[]() { Debugger_state = debugger_state::SINGLE_STEP; } };
		m_commands["exit"] = { "Exit debugger",
			[]() { Debugger_state = debugger_state::IDLE; } };
		m_commands["quit"] = { "Quit emulator",
			[]() { exit(-1); } };
		m_commands["break"] = { "Create breakpoint (break <addr>)",
			[]() {
				char *token = strtok(nullptr, " ");
				if (token != nullptr) {
					uint16_t address = (uint16_t)strtol(token, nullptr, 16);
					breakpoint b;

					b.m_type = breakpoint_type::BREAKPOINT;
					b.m_addr = address;
					b.m_enabled = true;
					Debugger_breakpoints.push_back(b);
				} } };
		m_commands["enable"] = { "Enable breakpoint (enable <bp#>)",
			[this]() {
				char *token = strtok(nullptr, " ");
				if (token != nullptr) {
					size_t val = strtol(token, nullptr, 10);
					if (val < Debugger_breakpoints.size()) {
						Debugger_breakpoints[val].m_enabled = true;
						AddLog("Breakpoint %d enabled.", val);
					} else {
						AddLog("%d is not a valid index into breakpoint list.", val);
					}
				} } };
		m_commands["disable"] = { "Disable breakpoint (disable <bp#>)",
			[this]() {
				char *token = strtok(nullptr, " ");
				if (token != nullptr) {
					size_t val = strtol(token, nullptr, 10);
					if (val < Debugger_breakpoints.size()) {
						Debugger_breakpoints[val].m_enabled = false;
						AddLog("Breakpoint %d disabled.", val);
					} else {
						AddLog("%d is not a valid index into breakpoint list.", val);
					}
				} } };
		m_commands["list"] = { "List breakpoints",
			[this]() {
				int index = 0;
				if (Debugger_breakpoints.size() == 0) {
					AddLog("No breakpoints defined.");
				} else {
					for (auto bp : Debugger_breakpoints) {
						if (bp.m_enabled) {
							AddLog("%d - 0x%04x enabled", index++, bp.m_addr);
						} else {
							AddLog("%d - 0x%04x disabled", index++, bp.m_addr);
						}
				} } } };
		m_commands["delete"] = { "Delete breakpoint (delete bp#)",
			[this]() {
				char *token = strtok(nullptr, " ");
				if (token != nullptr) {
					size_t val = strtol(token, nullptr, 10);
					if (val < Debugger_breakpoints.size()) {
						Debugger_breakpoints.erase(Debugger_breakpoints.begin() + val);
						AddLog("Breakpoint %d deleted.", val);
					} else {
						AddLog("%d not a valid index into breakpoint list.", val);
					}
				} } };
		m_commands["trace"] = { "Toggle trace to file.  (trace <filename> - filename"
			" is optional and if not specified, output will be to 'trace.log'",
			[this]() {
				char *token = strtok(nullptr, " ");
				if (Debugger_trace_fp != nullptr) {
					debugger_start_trace(token);
				} else {
					debugger_stop_trace();
				} } };
	}

	~DebuggerConsole()
	{
		clear_log();
		for (int i = 0; i < m_history.Size; i++) {
			free(m_history[i]);
		}
	}

	void clear_log()
	{
		for (int i = 0; i < m_items.Size; i++) {
			free(m_items[i]);
		}
		m_items.clear();
		m_scroll_to_bottom = true;
	}

	void AddLog(const char* fmt, ...) IM_PRINTFARGS(2)
	{
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		va_end(args);
		m_items.push_back(strdup(buf));
		m_scroll_to_bottom = true;
	}

	void Draw(const char* title, bool* p_open)
	{
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiSetCond_FirstUseEver);
		if (!ImGui::Begin(title, p_open)) {
			ImGui::End();
			return;
		}

		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::BeginPopupContextWindow()) {
			if (ImGui::Selectable("Clear")) {
				clear_log();
			}
			ImGui::EndPopup();
		}

		// Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
		// NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
		// You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
		// To use the clipper we could replace the 'for (int i = 0; i < Items.Size; i++)' loop with:
		//     ImGuiListClipper clipper(Items.Size);
		//     while (clipper.Step())
		//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		// However take note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access' property. We would need random-access on the post-filtered list.
		// A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when user changes the filter,
		// and appending newly elements as they are inserted. This is left as a task to the user until we can manage to improve this example code!
		// If your items are of variable size you may want to implement code similar to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		for (int i = 0; i < m_items.Size; i++) {
			const char* item = m_items[i];
			ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // A better implementation may store a type per-item. For the sample let's just parse the text.
			if (strstr(item, "[error]")) col = ImColor(1.0f, 0.4f, 0.4f, 1.0f);
			else if (strncmp(item, "# ", 2) == 0) col = ImColor(1.0f, 0.78f, 0.58f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::TextUnformatted(item);
			ImGui::PopStyleColor();
		}
		if (m_scroll_to_bottom) {
			ImGui::SetScrollHere();
		}
		m_scroll_to_bottom = false;
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::Separator();

		// Command-line
		if (ImGui::InputText("Input", m_input_buf, IM_ARRAYSIZE(m_input_buf),
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory,
			&TextEditCallbackStub, (void*)this)) {
			char* input_end = m_input_buf + strlen(m_input_buf);
			while (input_end > m_input_buf && input_end[-1] == ' ') input_end--; *input_end = 0;
			if (m_input_buf[0]) {
				ExecCommand();
			}
			strcpy(m_input_buf, "");
		}

		// Demonstrate keeping auto focus on the input box
		if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
		}

		ImGui::End();
	}

	void ExecCommand()
	{
		AddLog("# %s\n", m_input_buf);

		// Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
		m_history_pos = -1;
		for (int i = m_history.Size - 1; i >= 0; i--) {
			if (stricmp(m_history[i], m_input_buf) == 0) {
				free(m_history[i]);
				m_history.erase(m_history.begin() + i);
				break;
			}
		}
		m_history.push_back(strdup(m_input_buf));
		strcpy(Debugger_input, m_input_buf);

		debugger_get_status();
		debugger_get_disassembly(cpu.get_pc());

		// parse the debugger commands
		char *token = strtok(m_input_buf, " ");

		if (m_commands.find(token) != m_commands.end()) {
			m_commands[token].m_func();
		}

		// step to next statement
		// enable and disable breakpoints

		if ( !stricmp(token, "rwatch") ||
			!stricmp(token, "wwatch")) {
			// breakpoint at an address
			breakpoint_type bp_type = breakpoint_type::INVALID;

			if (token[0] == 'b') {
				bp_type = breakpoint_type::BREAKPOINT;
			}
			else if (token[0] == 'r') {
				bp_type = breakpoint_type::RWATCHPOINT;
			}
			else if (token[0] == 'w') {
				bp_type = breakpoint_type::WWATCHPOINT;
			}
			ASSERT(bp_type != breakpoint_type::INVALID);

		}
	}

	static int TextEditCallbackStub(ImGuiTextEditCallbackData* data) // In C++11 you are better off using lambdas for this sort of forwarding callbacks
	{
		DebuggerConsole* console = (DebuggerConsole*)data->UserData;
		return console->TextEditCallback(data);
	}

	int TextEditCallback(ImGuiTextEditCallbackData* data) {
		//AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
		switch (data->EventFlag) {
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			// Example of TEXT COMPLETION

			// Locate beginning of current word
			const char* word_end = data->Buf + data->CursorPos;
			const char* word_start = word_end;
			while (word_start > data->Buf) {
				const char c = word_start[-1];
				if (c == ' ' || c == '\t' || c == ',' || c == ';') {
					break;
				}
				word_start--;
			}

			// Build a list of candidates
			ImVector<const char*> candidates;
			for (auto cmd : m_commands) {
				if (strnicmp(cmd.first.c_str(), word_start, (int)(word_end - word_start)) == 0) {
					candidates.push_back(cmd.first.c_str());
				}
			}

			if (candidates.Size == 0) {
				// No match
				AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
			} else if (candidates.Size == 1) {
				// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
				data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
				data->InsertChars(data->CursorPos, candidates[0]);
				data->InsertChars(data->CursorPos, " ");
			} else {
				// Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display "CLEAR" and "CLASSIFY"
				int match_len = (int)(word_end - word_start);
				for (;;) {
					int c = 0;
					bool all_candidates_matches = true;
					for (int i = 0; i < candidates.Size && all_candidates_matches; i++) {
						if (i == 0) {
							c = toupper(candidates[i][match_len]);
						}
						else if (c != toupper(candidates[i][match_len])) {
							all_candidates_matches = false;
						}
					}
					if (!all_candidates_matches) {
						break;
					}
					match_len++;
				}

				if (match_len > 0) {
					data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
					data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
				}

				// List matches
				AddLog("Possible matches:\n");
				for (int i = 0; i < candidates.Size; i++)
					AddLog("- %s\n", candidates[i]);
			}

			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			// Example of HISTORY
			const int prev_history_pos = m_history_pos;
			if (data->EventKey == ImGuiKey_UpArrow) {
				if (m_history_pos == -1) {
					m_history_pos = m_history.Size - 1;
				}
				else if (m_history_pos > 0) {
					m_history_pos--;
				}
			}
			else if (data->EventKey == ImGuiKey_DownArrow) {
				if (m_history_pos != -1) {
					if (++m_history_pos >= m_history.Size) {
						m_history_pos = -1;
					}
				}
			}

			// A better implementation would preserve the data on the current input line along with cursor position.
			if (prev_history_pos != m_history_pos) {
				data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (m_history_pos >= 0) ? m_history[m_history_pos] : "");
				data->BufDirty = true;
			}
		}
		}
		return 0;
	}
};

struct MemoryEditor
{
	bool    Open;
	bool    AllowEdits;
	int     Rows;
	int     DataEditingAddr;
	bool    DataEditingTakeFocus;
	char    DataInput[32];
	char    AddrInput[32];

	MemoryEditor()
	{
		Open = true;
		Rows = 16;
		DataEditingAddr = -1;
		DataEditingTakeFocus = false;
		strcpy(DataInput, "");
		strcpy(AddrInput, "");
		AllowEdits = false;
	}

	void Draw(const char* title, int mem_size, size_t base_display_addr = 0)
	{
		if (ImGui::Begin(title, nullptr, default_window_flags)) {
			ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));

			int addr_digits_count = 0;
			for (int n = base_display_addr + mem_size - 1; n > 0; n >>= 4) {
				addr_digits_count++;
			}

			float glyph_width = ImGui::CalcTextSize("F").x;
			float cell_width = glyph_width * 3; // "FF " we include trailing space in the width to easily catch clicks everywhere

			float line_height = ImGui::GetTextLineHeight();
			int line_total_count = (int)((mem_size + Rows - 1) / Rows);
			ImGuiListClipper clipper(line_total_count, line_height);
			int visible_start_addr = clipper.DisplayStart * Rows;
			int visible_end_addr = clipper.DisplayEnd * Rows;

			bool data_next = false;

			if (!AllowEdits || DataEditingAddr >= mem_size)
				DataEditingAddr = -1;

			int data_editing_addr_backup = DataEditingAddr;
			if (DataEditingAddr != -1) {
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= Rows) { DataEditingAddr -= Rows; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Rows) { DataEditingAddr += Rows; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0) { DataEditingAddr -= 1; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1) { DataEditingAddr += 1; DataEditingTakeFocus = true; }
			}
			if ((DataEditingAddr / Rows) != (data_editing_addr_backup / Rows)) {
				// Track cursor movements
				float scroll_offset = ((DataEditingAddr / Rows) - (data_editing_addr_backup / Rows)) * line_height;
				bool scroll_desired = (scroll_offset < 0.0f && DataEditingAddr < visible_start_addr + Rows * 2) || (scroll_offset > 0.0f && DataEditingAddr > visible_end_addr - Rows * 2);
				if (scroll_desired)
					ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset);
			}

			bool draw_separator = true;
			for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible items
			{
				uint16_t addr = (uint16_t)(line_i * Rows);
				ImGui::Text("%0*lX: ", addr_digits_count, base_display_addr + addr);
				ImGui::SameLine();

				// Draw Hexadecimal
				float line_start_x = ImGui::GetCursorPosX();
				for (int n = 0; n < Rows && addr < mem_size; n++, addr++) {
					ImGui::SameLine(line_start_x + cell_width * n);

					if (DataEditingAddr == addr) {
						// Display text input on current byte
						ImGui::PushID(addr);
						struct FuncHolder
						{
							// FIXME: We should have a way to retrieve the text edit cursor position more easily in the API, this is rather tedious.
							static int Callback(ImGuiTextEditCallbackData* data)
							{
								int* p_cursor_pos = (int*)data->UserData;
								if (!data->HasSelection())
									*p_cursor_pos = data->CursorPos;
								return 0;
							}
						};
						int cursor_pos = -1;
						bool data_write = false;
						if (DataEditingTakeFocus) {
							ImGui::SetKeyboardFocusHere();
							sprintf(AddrInput, "%0*lX", addr_digits_count, base_display_addr + addr);
							sprintf(DataInput, "%02X", memory_read(addr));
						}
						ImGui::PushItemWidth(ImGui::CalcTextSize("FF").x);
						ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysInsertMode | ImGuiInputTextFlags_CallbackAlways;
						if (ImGui::InputText("##data", DataInput, 32, flags, FuncHolder::Callback, &cursor_pos))
							data_write = data_next = true;
						else if (!DataEditingTakeFocus && !ImGui::IsItemActive())
							DataEditingAddr = -1;
						DataEditingTakeFocus = false;
						ImGui::PopItemWidth();
						if (cursor_pos >= 2)
							data_write = data_next = true;
						//if (data_write)
						//{
						//	int data;
						//	if (sscanf(DataInput, "%X", &data) == 1)
						//		memory_write(addr, data);
						//}
						ImGui::PopID();
					}
					else
					{
						uint8_t val = 0;
						if (addr < 0xc000 || addr > 0xc0ff) {
							val = memory_read(addr);
						}

						ImGui::Text("%02X ", val);
						if (AllowEdits && ImGui::IsItemHovered() &&
							ImGui::IsMouseClicked(0))
						{
							DataEditingTakeFocus = true;
							DataEditingAddr = addr;
						}
					}
				}

				ImGui::SameLine(line_start_x + cell_width * Rows + glyph_width * 2);

				if (draw_separator)
				{
					ImVec2 screen_pos = ImGui::GetCursorScreenPos();
					ImGui::GetWindowDrawList()->AddLine(ImVec2(screen_pos.x - glyph_width,
						screen_pos.y - 9999),
						ImVec2(screen_pos.x - glyph_width, screen_pos.y + 9999),
						ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]));
					draw_separator = false;
				}

				// Draw ASCII values
				addr = (uint16_t)(line_i * Rows);
				for (int n = 0; n < Rows && addr < mem_size; n++, addr++) {
					if (n > 0) ImGui::SameLine();
					int c = '?';
					if (addr < 0xc000 || addr > 0xc0ff) {
						c = memory_read(addr);
					}
					ImGui::Text("%c", (c >= 32 && c < 128) ? c : '.');
				}
			}
			clipper.End();
			ImGui::PopStyleVar(2);

			ImGui::EndChild();

			if (data_next && DataEditingAddr < mem_size) {
				DataEditingAddr = DataEditingAddr + 1;
				DataEditingTakeFocus = true;
			}

			ImGui::Separator();

			ImGui::AlignFirstTextHeightToWidgets();
			ImGui::SameLine();
			ImGui::Text("Range %0*X..%0*X", addr_digits_count,
				(int)base_display_addr, addr_digits_count,
				(int)base_display_addr + mem_size - 1);
			ImGui::SameLine();
			ImGui::PushItemWidth(70);
			if (ImGui::InputText("##addr", AddrInput, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
				int goto_addr;
				if (sscanf(AddrInput, "%X", &goto_addr) == 1) {
					goto_addr -= base_display_addr;
					if (goto_addr >= 0 && goto_addr < mem_size) {
						ImGui::BeginChild("##scrolling");
						ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (goto_addr / Rows) * ImGui::GetTextLineHeight());
						ImGui::EndChild();
						DataEditingAddr = goto_addr;
						DataEditingTakeFocus = true;
					}
				}
			}
			ImGui::PopItemWidth();

			// keeps focus in input box
			if (ImGui::IsItemHovered() ||
				(ImGui::IsRootWindowOrAnyChildFocused() &&
					!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
				ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
			}
		}
		ImGui::End();
	}
};

static void debugger_display_memory()
{
	static MemoryEditor memory_editor;

	memory_editor.Draw("Memory", 0x10000, 0);
}

static void debugger_display_soft_switch()
{
	if (ImGui::Begin("Soft Switches", nullptr, default_window_flags)) {
		if (Memory_state & RAM_CARD_BANK2) {
			ImGui::Text("Bank1/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Bank2");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Bank1");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Bank2");
		}
		if (Memory_state & RAM_CARD_READ) {
			ImGui::Text("RCard Write/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "RCard Read");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "RCard Write");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/RCard Read");
		}
		if (Memory_state & RAM_AUX_MEMORY_READ) {
			ImGui::Text("Main Read/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Aux Read");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Main Read");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Aux Read");
		}
		if (Memory_state & RAM_AUX_MEMORY_WRITE) {
			ImGui::Text("Main Write/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Aux Write");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Main Write");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Aux Write");
		}
		if (Memory_state & RAM_SLOTCX_ROM) {
			ImGui::Text("Internal Rom/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Cx Rom");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Internal Rom");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Cx Rom");
		}
		if (Memory_state & RAM_ALT_ZERO_PAGE) {
			ImGui::Text("Zero Page/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Alt zp");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Zero Page");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Alt zp");
		}
		if (Memory_state & RAM_SLOTC3_ROM) {
			ImGui::Text("Internal Rom/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "C3 Rom");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Internal Rom");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/C3 Rom");
		}
		if (Memory_state & RAM_80STORE) {
			ImGui::Text("Normal/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "80Store");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Normal");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/80Store");
		}
		if (Video_mode & VIDEO_MODE_TEXT) {
			ImGui::Text("Graphics/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Text");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Graphics");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Text");
		}
		if (Video_mode & VIDEO_MODE_MIXED) {
			ImGui::Text("Not Mixed/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Mixed");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Not Mixed");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Mixed");
		}
		if (!(Video_mode & VIDEO_MODE_PAGE2)) {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Page 1");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Page 2");
		}
		else {
			ImGui::Text("Page 1/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Page 2");
		}
		if (Video_mode & VIDEO_MODE_HIRES) {
			ImGui::Text("Lores/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Hires");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lores");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Hires");
		}
		if (Video_mode & VIDEO_MODE_ALTCHAR) {
			ImGui::Text("Reg char/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Alt char");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Reg char");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/Alt char");
		}
		if (Video_mode & VIDEO_MODE_80COL) {
			ImGui::Text("40/");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "80");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "40");
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::Text("/80");
		}
	}
	ImGui::End();
}

// display breakpoints
static void debugger_display_breakpoints()
{
	if (ImGui::Begin("Breakpoints", nullptr, default_window_flags)) {
		// for now, just disassm from the current pc
		for (size_t i = 0; i < Debugger_breakpoints.size(); i++) {
			ImGui::Text("%-3d", i);
			ImGui::SameLine();
			switch (Debugger_breakpoints[i].m_type) {
			case breakpoint_type::BREAKPOINT:
				ImGui::Text("%-5s", "Bp");
				break;

			case breakpoint_type::RWATCHPOINT:
				ImGui::Text("%-5s", "Rwp");
				break;

			case breakpoint_type::WWATCHPOINT:
				ImGui::Text("%-5s", "Wwp");
				break;

			case breakpoint_type::INVALID:
				break;
			}

			ImGui::SameLine();
			ImGui::Text("$%-6x", Debugger_breakpoints[i].m_addr);
			ImGui::SameLine();
			ImGui::Text("%s", Debugger_breakpoints[i].m_enabled == true ? "enabled" : "disabled");
			ImGui::NewLine();
		}
	}
	ImGui::End();
}

// display the disassembly in the disassembly window
static void debugger_display_disasm()
{
	static bool column_set = false;
	auto addr = cpu.get_pc();
	if (ImGui::Begin("Disassembly", nullptr, default_window_flags)) {
		ImGui::Columns(2);
		if (column_set == false) {
			ImGui::SetColumnOffset(1, ImGui::GetColumnOffset(1) + 100.0f);
			column_set = true;
		}

		ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));

		float line_height = ImGui::GetTextLineHeight();
		int line_total_count = 1000;
		ImGuiListClipper clipper(line_total_count, line_height);

		for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible items
		{
			if (addr == cpu.get_pc()) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
			}
			auto size = debugger_get_disassembly(addr);
			ImGui::Text("%s", Debugger_disassembly_line);
			if (addr == cpu.get_pc()) {
				ImGui::PopStyleColor();
			}
			addr += size;
		}
		clipper.End();

		ImGui::EndChild();

		ImGui::NextColumn();
		ImGui::Text("A  = $%02X", cpu.get_acc());
		ImGui::Text("X  = $%02X", cpu.get_x());
		ImGui::Text("Y  = $%02X", cpu.get_y());
		ImGui::Text("PC = $%04X", cpu.get_pc());
		ImGui::Text("SP = $%04X", cpu.get_sp() + 0x100);

		// keeps focus on input box
		if (ImGui::IsItemHovered() ||
			(ImGui::IsRootWindowOrAnyChildFocused() &&
				!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
		}
	}
	ImGui::End();
}

static void debugger_display_console()
{
	static DebuggerConsole console;
	console.Draw("Console", nullptr);
}

void debugger_enter()
{
	Debugger_state = debugger_state::WAITING_FOR_INPUT;
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
}

bool debugger_process()
{
	bool continue_execution = true;

	// check on breakpoints
	if (Debugger_state == debugger_state::IDLE ||
		Debugger_state == debugger_state::SHOW_ALL) {
		for (size_t i = 0; i < Debugger_breakpoints.size(); i++) {
			if (Debugger_breakpoints[i].m_enabled == true) {
				switch (Debugger_breakpoints[i].m_type) {
				case breakpoint_type::BREAKPOINT:
					if (cpu.get_pc() == Debugger_breakpoints[i].m_addr) {
						debugger_enter();
						continue_execution = false;
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

	else if (Debugger_state == debugger_state::WAITING_FOR_INPUT) {
		continue_execution = false;
	}

	else if (Debugger_state == debugger_state::SINGLE_STEP) {
		Debugger_state = debugger_state::WAITING_FOR_INPUT;
	}

	if (Debugger_trace_fp != nullptr) {
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
	debugger_display_memory();
	debugger_display_disasm();
	debugger_display_breakpoints();
	debugger_display_soft_switch();
	debugger_display_console();
}
