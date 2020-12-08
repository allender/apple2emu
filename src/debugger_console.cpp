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
#include "debugger_console.h"
#include "imgui.h"


debugger_console::debugger_console()
{
	clear_log();
	memset(m_input_buf, 0, sizeof(m_input_buf));
	m_history_pos = -1;
	m_commands["help"] = { "Show help ",
		[this](char *) {
			for (auto const &cmd : m_commands) {
				add_log("%s - %s", cmd.first.c_str(), cmd.second.m_help.c_str());
			}
	} };

	m_reset_window = false;
}

debugger_console::~debugger_console()
{
	clear_log();
	for (size_t i = 0; i < m_history.size(); i++) {
		free(m_history[i]);
	}
}

void debugger_console::add_command(const char *name, const char *help, std::function<void(char *)> func)
{
	console_command cmd;

	cmd.m_help = help;
	cmd.m_func = func;
	m_commands[name] = cmd;
}

void debugger_console::clear_log()
{
	for (size_t i = 0; i < m_items.size(); i++) {
		free(m_items[i]);
	}
	m_items.clear();
	m_scroll_to_bottom = true;
}

void debugger_console::add_log(const char* fmt, ...)
{
	char buf[max_line_size];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, max_line_size, fmt, args);
	buf[max_line_size - 1] = 0;
	va_end(args);
	m_items.push_back(strdup(buf));
	m_scroll_to_bottom = true;
}

void debugger_console::draw(const char* title, bool* p_open)
{
	ImGuiCond condition = ImGuiCond_FirstUseEver;
	if (m_reset_window == true) {
		condition = ImGuiCond_Always;
		m_reset_window = false;
	}

	ImGui::SetNextWindowSize(ImVec2(562,180), condition);
	ImGui::SetNextWindowPos(ImVec2(2, 582), condition);
	if (!ImGui::Begin(title, p_open, 0)) {
		ImGui::End();
		return;
	}

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow()) {
		if (ImGui::Selectable("Clear")) {
			clear_log();
		}
		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
	for (size_t i = 0; i < m_items.size(); i++) {
		const char* item = m_items[i];
		ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
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
	if (ImGui::InputText("Input", m_input_buf, max_line_size,
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory,
		[](ImGuiTextEditCallbackData *data) {
			debugger_console* console = (debugger_console *)data->UserData;
			return console->text_edit_callback(data);
		}, (void*)this)) {

		char* input_end = m_input_buf + strlen(m_input_buf);
		while (input_end > m_input_buf && input_end[-1] == ' ') {
			input_end--;
			*input_end = 0;
		}

		if (m_input_buf[0]) {
			execute_command();
		}
		strcpy(m_input_buf, "");
	}

	// Demonstrate keeping auto focus on the input box
	if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
		ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
	}

	ImGui::End();
}

void debugger_console::execute_command(const char *command)
{
	m_history_pos = -1;

	// don't insert duplicate items into history.  Acts like
	// ignoredupes in bash.
	if (command == nullptr && (m_history.size() == 0 || stricmp(m_history[m_history.size()-1], m_input_buf))) {
		m_history.push_back(strdup(m_input_buf));
	}

	// parse the debugger commands
	const char *token = nullptr;
	if (command != nullptr) {
		strcpy(m_input_buf, command);
	}
	token = strtok(m_input_buf, " ");

	if (m_commands.find(token) != m_commands.end()) {
		m_commands[token].m_func(m_input_buf);
	}
	if (command != nullptr) {
		m_input_buf[0] = '\0';
	}
}

int debugger_console::text_edit_callback(ImGuiTextEditCallbackData* data) {
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
		std::vector<const char*> candidates;
		for (auto cmd : m_commands) {
			if (strnicmp(cmd.first.c_str(), word_start, (int)(word_end - word_start)) == 0) {
				candidates.push_back(cmd.first.c_str());
			}
		}

		if (candidates.size() == 0) {
			// No match
			add_log("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
		} else if (candidates.size() == 1) {
			// Single match. Delete the beginning of the word
			// and replace it entirely so we've got nice casing
			data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
			data->InsertChars(data->CursorPos, candidates[0]);
			data->InsertChars(data->CursorPos, " ");
		} else {
			// Multiple matches. Complete as much as we can, so
			// inputing "C" will complete to "CL" and display
			// "CLEAR" and "CLASSIFY"
			int match_len = (int)(word_end - word_start);
			for (;;) {
				int c = 0;
				bool all_candidates_matches = true;
				for (size_t i = 0; i < candidates.size() && all_candidates_matches; i++) {
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
			add_log("Possible matches:\n");
			for (size_t i = 0; i < candidates.size(); i++)
				add_log("- %s\n", candidates[i]);
		}

		break;
	}
	case ImGuiInputTextFlags_CallbackHistory:
	{
		// Example of HISTORY
		const size_t prev_history_pos = m_history_pos;
		if (data->EventKey == ImGuiKey_UpArrow) {
			if (m_history_pos == -1) {
				m_history_pos = m_history.size() - 1;
			}
			else if (m_history_pos > 0) {
				m_history_pos--;
			}
		}
		else if (data->EventKey == ImGuiKey_DownArrow) {
			if (m_history_pos != -1) {
				if (++m_history_pos >= (int)m_history.size()) {
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
