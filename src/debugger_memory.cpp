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
#include "debugger_memory.h"
#include "memory.h"
#include "imgui.h"

debugger_memory_editor::debugger_memory_editor()
{
	m_open = true;
	m_reset_window = false;
	m_show_ram = memory_high_read_type::READ_ROM;
	m_bank_num = memory_high_read_bank::READ_BANK1;
	m_columns = 16;
	m_data_editing_address = -1;
	m_data_editing_take_focus = false;
	strcpy(m_data_input, "");
	strcpy(m_addr_input, "");
	m_allow_edits = false;
}

debugger_memory_editor::~debugger_memory_editor()
{
}

void debugger_memory_editor::draw(const char* title, int mem_size, size_t base_display_addr)
{
	ImGuiSetCond condition = ImGuiSetCond_FirstUseEver;
	if (m_reset_window) {
		condition = ImGuiSetCond_Always;
		m_reset_window = false;
	}

	ImGui::SetNextWindowSize(ImVec2(550, 332), condition);
	ImGui::SetNextWindowPos(ImVec2(5, 428), condition);

	if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_ShowBorders)) {
		ImGui::BeginChild("##scrolling", ImVec2(0, -(ImGui::GetItemsLineHeightWithSpacing() * 2)));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));

		int addr_digits_count = 0;
		for (int n = base_display_addr + mem_size - 1; n > 0; n >>= 4) {
			addr_digits_count++;
		}

		float glyph_width = ImGui::CalcTextSize("F").x;
		// "FF " we include trailing space in the width to easily catch clicks everywhere
		float cell_width = glyph_width * 3;
		float line_height = ImGui::GetTextLineHeight();
		int line_total_count = (int)((mem_size + m_columns - 1) / m_columns);
		ImGuiListClipper clipper(line_total_count, line_height);
		int visible_start_addr = clipper.DisplayStart * m_columns;
		int visible_end_addr = clipper.DisplayEnd * m_columns;

		bool data_next = false;

		if (!m_allow_edits || m_data_editing_address >= mem_size)
			m_data_editing_address = -1;

		int data_editing_addr_backup = m_data_editing_address;
		if (m_data_editing_address != -1) {
			if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && m_data_editing_address >= m_columns) {
				m_data_editing_address -=m_columns;
				m_data_editing_take_focus = true;
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && m_data_editing_address < mem_size -m_columns) {
				m_data_editing_address += m_columns;
				m_data_editing_take_focus = true;
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && m_data_editing_address > 0) {
				m_data_editing_address -= 1;
				m_data_editing_take_focus = true;
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && m_data_editing_address < mem_size - 1) {
				m_data_editing_address += 1;
				m_data_editing_take_focus = true;
			}
		}
		if ((m_data_editing_address / m_columns) != (data_editing_addr_backup / m_columns)) {
			// Track cursor movements
			float scroll_offset = ((m_data_editing_address / m_columns) - (data_editing_addr_backup / m_columns)) * line_height;
			bool scroll_desired = (scroll_offset < 0.0f && m_data_editing_address < visible_start_addr + m_columns * 2) || (scroll_offset > 0.0f && m_data_editing_address > visible_end_addr - m_columns * 2);
			if (scroll_desired)
				ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset);
		}

		bool draw_separator = true;
		for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible items
		{
			uint16_t addr = (uint16_t)(line_i * m_columns);
			ImGui::Text("%0*lX: ", addr_digits_count, base_display_addr + addr);
			ImGui::SameLine();

			// Draw Hexadecimal
			float line_start_x = ImGui::GetCursorPosX();
			for (int n = 0; n < m_columns && addr < mem_size; n++, addr++) {
				ImGui::SameLine(line_start_x + cell_width * n);

				if (m_data_editing_address == addr) {
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
					if (m_data_editing_take_focus) {
						ImGui::SetKeyboardFocusHere();
						sprintf(m_addr_input, "%0*lX", addr_digits_count, base_display_addr + addr);
						sprintf(m_data_input, "%02X", memory_read(addr));
					}
					ImGui::PushItemWidth(ImGui::CalcTextSize("FF").x);
					ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysInsertMode | ImGuiInputTextFlags_CallbackAlways;
					if (ImGui::InputText("##data", m_data_input, 32, flags, FuncHolder::Callback, &cursor_pos))
						data_write = data_next = true;
					else if (!m_data_editing_take_focus && !ImGui::IsItemActive())
						m_data_editing_address = -1;
					m_data_editing_take_focus = false;
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
						val = memory_read(addr, m_show_ram, m_bank_num);
					}

					ImGui::Text("%02X ", val);
					if (m_allow_edits && ImGui::IsItemHovered() &&
						ImGui::IsMouseClicked(0))
					{
						m_data_editing_take_focus = true;
						m_data_editing_address = addr;
					}
				}
			}

			ImGui::SameLine(line_start_x + cell_width * m_columns + glyph_width * 2);

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
			addr = (uint16_t)(line_i * m_columns);
			for (int n = 0; n < m_columns && addr < mem_size; n++, addr++) {
				if (n > 0) ImGui::SameLine();
				int c = '?';
				if (addr < 0xc000 || addr > 0xc0ff) {
					c = memory_read(addr, m_show_ram, m_bank_num);
				}
				ImGui::Text("%c", (c >= 32 && c < 128) ? c : '.');
			}
		}
		clipper.End();
		ImGui::PopStyleVar(2);

		ImGui::EndChild();

		if (data_next && m_data_editing_address < mem_size) {
			m_data_editing_address = m_data_editing_address + 1;
			m_data_editing_take_focus = true;
		}

		ImGui::Separator();

		ImGui::AlignFirstTextHeightToWidgets();
		ImGui::SameLine();
		ImGui::Text("Range %0*X..%0*X", addr_digits_count,
			(int)base_display_addr, addr_digits_count,
			(int)base_display_addr + mem_size - 1);
		ImGui::SameLine();
		ImGui::PushItemWidth(70);
		if (ImGui::InputText("##addr", m_addr_input, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			int goto_addr;
			if (sscanf(m_addr_input, "%X", &goto_addr) == 1) {
				goto_addr -= base_display_addr;
				if (goto_addr >= 0 && goto_addr < mem_size) {
					ImGui::BeginChild("##scrolling");
					ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (goto_addr / m_columns) * ImGui::GetTextLineHeight());
					ImGui::EndChild();
					m_data_editing_address = goto_addr;
					m_data_editing_take_focus = true;
				}
			}
		}
		ImGui::PopItemWidth();

		// add in radio buttons for displaying specifics kinds of RAM in the
		// memoory window
		int rom_type = static_cast<int>(m_show_ram);
		ImGui::SameLine();
		ImGui::RadioButton("ROM", &rom_type, static_cast<int>(memory_high_read_type::READ_ROM));
		ImGui::SameLine();
		ImGui::RadioButton("Bank RAM", &rom_type, static_cast<int>(memory_high_read_type::READ_RAM));
		m_show_ram = static_cast<memory_high_read_type>(rom_type);

		if (m_show_ram == memory_high_read_type::READ_RAM) {
			int ram_bank = static_cast<int>(m_bank_num);
			ImGui::SameLine();
			ImGui::RadioButton("Bank 1", &ram_bank, static_cast<int>(memory_high_read_bank::READ_BANK1));
			ImGui::SameLine();
			ImGui::RadioButton("Bank 2", &ram_bank, static_cast<int>(memory_high_read_bank::READ_BANK2));
			m_bank_num = static_cast<memory_high_read_bank>(ram_bank);
		}

		//

		// keeps focus in input box
		//if (ImGui::IsItemHovered() ||
		//	(ImGui::IsRootWindowOrAnyChildFocused() &&
		//		!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
		//	ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
		//}
	}
	ImGui::End();
}

