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
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "SDL_scancode.h"
#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "debugger.h"
#include "debugger_disasm.h"
#include "imgui.h"
#include "keyboard.h"
#include "memory.h"
#include "video.h"


static const int Max_symtable_line = 256;

// these string formats need to match the addressing modes
// defined in 6502.h
const char *debugger_disasm::m_addressing_format_string[] = {
	"",           // NO_MODE
	"",           // ACCUMULATOR_MODE
	"#$%02X",     // IMMEDIATE_MODE
	"",           // IMPLIED_MODE
	"$%02X",      // RELATIVE_MODE
	"$%04X  ",    // ABSOLUTE_MODE
	"$%02X    ",  // ZERO_PAGE_MODE
	"$(%04X)",    // INDIRECT_MODE
	"$(%02X)",    // INDIRECT_ZP_MODE
	"$%04X,X",    // X_INDEXED_MODE
	"$%04X,Y",    // Y_INDEXED_MODE
	"$%02X,X",    // ZERO_PAGES_INDEXED_MODE
	"$%02X,Y",    // ZERO_PAGED_INDEXED_MODE_Y
	"($%02X,X)",  // INDEXED_INDIRECT_MODE'
	"",           // ABSOLUTE_INDEXED_INDIRECT_MODE
	"$(%02X),Y",  // INDIRECT_INDEXED_MODE
};

void debugger_disasm::add_symtable(const char *filename)
{
	symtable_map table;
	FILE *fp = fopen(filename, "rt");
	if (fp != nullptr) {
		int line_num = 0;
		int prev_val = -1;
		while (!feof(fp)) {
			line_num++;
			char line[Max_symtable_line], *ptr;
			ptr = fgets(line, Max_symtable_line, fp);
			if (ptr != nullptr) {
				while (isspace(*ptr)) {
					ptr++;
				}
				char *addr = strtok(ptr, "\t ");
				char *label = strtok(nullptr, " \t\n");
				if (addr == nullptr || label == nullptr) {
					continue;
				}
				uint16_t val = (uint16_t)strtol(addr, nullptr, 16);
				if (prev_val != -1 && val < prev_val) {
					printf("invalid valud at line %d - less than previous value\n", line_num);
					continue;
				}
				table[val] = strdup(label);
				prev_val = val;
			}
		}
		fclose(fp);
	}
	m_symbol_tables.push_back(table);
}

const char *debugger_disasm::find_symbol(uint16_t addr)
{
	if (Debugger_use_sym_tables == false) {
		return nullptr;
	}

	for (auto table: m_symbol_tables) {
		auto element = table.find(addr);
		if (element != table.end()) {
			return element->second;
		}
	}
	return nullptr;
}

debugger_disasm::debugger_disasm() : m_console(nullptr),
									 m_reset_window(false)
{
	// load in symbol tables
	add_symtable("symtables/apple2e_sym.txt");
}


debugger_disasm::~debugger_disasm()
{
}

// disassemble the given address into the buffer
uint8_t debugger_disasm::get_disassembly(uint16_t addr)
{
	char internal_buffer[m_disassembly_line_length];
	m_disassembly_line[0] = '\0';

	// get the opcode at the address and from there we have the mode
	cpu_6502::opcode_info *opcode = cpu.get_opcode(memory_read(addr, true));
	if (opcode->m_mnemonic == '    ') {
		// invalid opcode.  just print ?? and continue
		strcpy(m_disassembly_line, "???");
	}
	else {
		sprintf(internal_buffer, "%04X:", addr);
		strcat(m_disassembly_line, internal_buffer);

		for (uint8_t j = 0; j < 3; j++) {
			if (j < opcode->m_size) {
				sprintf(internal_buffer, "%02X ", memory_read(addr + j));
			}
			else {
				sprintf(internal_buffer, "   ");
			}
			strcat(m_disassembly_line, internal_buffer);
		}

		// print opcode
		sprintf(internal_buffer, "   %c%c%c ", opcode->m_mnemonic >> 24,
			opcode->m_mnemonic >> 16,
			opcode->m_mnemonic >> 8);

		strcat(m_disassembly_line, internal_buffer);

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
			sprintf(internal_buffer, m_addressing_format_string[static_cast<uint8_t>(mode)], addressing_val);
			strcat(m_disassembly_line, internal_buffer);
			if (0) {
				switch (mode) {
				case cpu_6502::addr_mode::ACCUMULATOR_MODE:
				case cpu_6502::addr_mode::IMMEDIATE_MODE:
				case cpu_6502::addr_mode::IMPLIED_MODE:
				case cpu_6502::addr_mode::NO_MODE:
				case cpu_6502::addr_mode::NUM_ADDRESSING_MODES:
				case cpu_6502::addr_mode::ABSOLUTE_INDEXED_INDIRECT_MODE:
				case cpu_6502::addr_mode::INDIRECT_ZP_MODE:
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

				case cpu_6502::addr_mode::INDIRECT_MODE:
					addressing_val = (memory_read(addressing_val + 2) << 8) | memory_read(addressing_val + 1);
					sprintf(internal_buffer, "\t$%04X", addressing_val);
					strcat(m_disassembly_line, internal_buffer);
					break;

				case cpu_6502::addr_mode::ABSOLUTE_MODE:
				case cpu_6502::addr_mode::ZERO_PAGE_MODE:
				{
					const char *mem_str = find_symbol(addressing_val);
					if (opcode->m_mnemonic != 'JSR ' && opcode->m_mnemonic != 'JMP ') {
						if (mem_str != nullptr) {
							sprintf(internal_buffer, "\t%s", mem_str);
							strcat(m_disassembly_line, internal_buffer);
						} else {
							sprintf(internal_buffer, "\t$%04X", addressing_val);
							strcat(m_disassembly_line, internal_buffer);
						}
						if (((addressing_val >> 8) & 0xff) != 0xc0 && mem_str == nullptr) {
							mem_value = memory_read(addressing_val);
							sprintf(internal_buffer, ": $%02X", mem_value);
							strcat(m_disassembly_line, internal_buffer);
							if (isprint(mem_value)) {
								sprintf(internal_buffer, " (%c)", mem_value);
								strcat(m_disassembly_line, internal_buffer);
							}
						}
					} else {
						if (mem_str != nullptr) {
							sprintf(internal_buffer, "\t%s", mem_str);
							strcat(m_disassembly_line, internal_buffer);
						}
					}
					break;
				}

					// indexed and zero page indexed are the same
				case cpu_6502::addr_mode::X_INDEXED_MODE:
				case cpu_6502::addr_mode::ZP_INDEXED_MODE:
				case cpu_6502::addr_mode::Y_INDEXED_MODE:
				case cpu_6502::addr_mode::ZP_INDEXED_MODE_Y:
					if (mode == cpu_6502::addr_mode::X_INDEXED_MODE ||
							mode == cpu_6502::addr_mode::ZP_INDEXED_MODE) {
						addressing_val += cpu.get_x();
					} else {
						addressing_val += cpu.get_y();
					}

					// don't print out memory value if this is softswitch area
					sprintf(internal_buffer, "\t$%04X", addressing_val);
					strcat(m_disassembly_line, internal_buffer);
					if (((addressing_val >> 8) & 0xff) != 0xc0) {
						mem_value = memory_read(addressing_val);
						sprintf(internal_buffer, ": $%02X", mem_value);
						strcat(m_disassembly_line, internal_buffer);
						if (isprint(mem_value)) {
							sprintf(internal_buffer, " (%c)", mem_value);
							strcat(m_disassembly_line, internal_buffer);
						}
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
					addressing_val = (memory_read(addressing_val + 2) << 8) | memory_read(addressing_val + 1);
					addressing_val += cpu.get_y();
					mem_value = memory_read(addressing_val);
					sprintf(internal_buffer, "\t$%04X: $%02X", addressing_val, mem_value);
					strcat(m_disassembly_line, internal_buffer);
					if (isprint(mem_value)) {
						sprintf(internal_buffer, " (%c)", mem_value);
						strcat(m_disassembly_line, internal_buffer);
					}
					break;
				}
			}
		}
	}

	return opcode->m_size;
}

void debugger_disasm::draw(const char *title, uint16_t pc)
{
	if (pc != m_break_addr) {
		m_break_addr = m_current_addr = pc;
	}

	ImGuiCond condition = ImGuiCond_FirstUseEver;
	if (m_reset_window) {
		condition = ImGuiCond_Always;
		m_reset_window = false;
	}

	ImGui::SetNextWindowSize(ImVec2(546, 578), condition);
	ImGui::SetNextWindowPos(ImVec2(570, 8), condition);

	// get the styling so we can get access to color values
	ImGuiStyle &style = ImGui::GetStyle();
	ImGuiIO &io = ImGui::GetIO();

	if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoScrollbar |
				         ImGuiWindowFlags_NoScrollWithMouse)) {
		ImGui::Columns(2);
		ImGui::SetColumnOffset(1, ImGui::GetWindowContentRegionWidth() - 200.0f);

		// if window is focued, processes arrow keys to move disassembly
		if (ImGui::IsWindowFocused()) {
			uint16_t new_addr = 0xffff;

			// take care of key presses
			if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) ||
					(ImGui::IsWindowHovered() && io.MouseWheel > 0.0f)) {
				new_addr = memory_find_previous_opcode_addr(m_current_addr, 1);
				if (new_addr == m_current_addr) {
					new_addr = m_current_addr - 1;
				}
				m_current_addr = new_addr;
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) ||
					(ImGui::IsWindowHovered() && io.MouseWheel < 0.0f)) {
				// get opcode size and move to new address
				cpu_6502::opcode_info *opcode = cpu.get_opcode(memory_read(m_current_addr));
				if (cpu.get_opcode(memory_read(m_current_addr + opcode->m_size))->m_size > 0) {
					new_addr = m_current_addr + opcode->m_size;
				}
				m_current_addr = new_addr;
			}

			// commands that get sent to console (if one is attached)
			if (m_console != nullptr) {
				if (ImGui::IsKeyPressed(SDL_SCANCODE_F5)) {
					m_console->execute_command(continue_command_name);
				} else if (ImGui::IsKeyPressed(SDL_SCANCODE_F6)) {
					// step over (or next)
					m_console->execute_command(step_command_name);
				} else if (ImGui::IsKeyPressed(SDL_SCANCODE_F7)) {
					// step into (or just step)
					m_console->execute_command(next_command_name);
				} else if (ImGui::IsKeyPressed(SDL_SCANCODE_F9)) {
					char command[256];
					sprintf(command, "%s %x", break_command_name, m_current_addr);
					m_console->execute_command(command);
				}
			}
		}
		float line_height = ImGui::GetTextLineHeightWithSpacing();
		int line_total_count = 0xffff;
		ImGuiListClipper clipper;

		// scan backwards in memory until we find the opcode
		// that will put current address in the middle of the screen
		clipper.Begin(line_total_count, line_height);
		while (clipper.Step()) {
			int middle_line = (clipper.DisplayEnd - clipper.DisplayStart) / 2;
			auto addr = memory_find_previous_opcode_addr(m_current_addr, middle_line);

			for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) {
				auto size = get_disassembly(addr);
				auto f = std::find_if(Debugger_breakpoints.begin(),
					Debugger_breakpoints.end(),
					[addr](breakpoint &bp) { return bp.m_addr == addr; });

				// determine color to use.  Breakpoints show in red (lighter red
				// for disabled ones, current broken on address in bright white
				// and then eveyrhtnig else.  Always display current line
				// in bold white
				if (addr == m_current_addr) {
					ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text]);
				} else if (f != Debugger_breakpoints.end() && f->m_enabled == true) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				} else if (f != Debugger_breakpoints.end() && f->m_enabled == false) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
				} else if (addr == m_break_addr) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
				}
				ImGui::Text("%s", m_disassembly_line);
				ImGui::PopStyleColor();
				addr += size;
			}
		}

		// need to reset the row back to the top since new column just
		// moves to the next column
		ImGui::NextColumn();
		ImGui::SetCursorPosY(0.0f);
		ImGui::Text("A  = $%02X", cpu.get_acc());
		ImGui::Text("X  = $%02X", cpu.get_x());
		ImGui::Text("Y  = $%02X", cpu.get_y());
		ImGui::Text("PC = $%04X", cpu.get_pc());
		ImGui::Text("SP = $%04X", cpu.get_sp() + 0x100);

		ImGui::NewLine();
		uint8_t status = cpu.get_status();
		ImGui::Text("%c%c%c%c%c%c%c%c\n%1d%1d%1d%1d%1d%1d%1d%1d = $%2x",
			(status >> 7) & 1 ? 'S' : 's',
			(status >> 6) & 1 ? 'V' : 'v',
			(status >> 5) & 1 ? '1' : '0',
			(status >> 4) & 1 ? 'B' : 'b',
			(status >> 3) & 1 ? 'D' : 'd',
			(status >> 2) & 1 ? 'I' : 'i',
			(status >> 1) & 1 ? 'Z' : 'z',
			(status >> 0) & 1 ? 'C' : 'c',
			(status >> 7) & 1,
			(status >> 6) & 1,
			(status >> 5) & 1,
			(status >> 4) & 1,
			(status >> 3) & 1,
			(status >> 2) & 1,
			(status >> 1) & 1,
			(status & 1),
			status);

		ImGui::NewLine();
		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Soft Switches")) {
			if (Memory_state & RAM_CARD_BANK2) {
				ImGui::Text("%-15s", "Bank1");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Bank2");
			}
			else {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Bank1");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Bank2");
			}
			if (Memory_state & RAM_CARD_READ) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "RCard Write");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "RCard Read");
			}
			else {
				ImGui::Text("%-15s", "RCard Write");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "RCard Read");
			}
			if (Memory_state & RAM_AUX_MEMORY_READ) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Main Read");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Aux Read");
			}
			else {
				ImGui::Text("%-15s", "Main Read");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Aux Read");
			}
			if (Memory_state & RAM_AUX_MEMORY_WRITE) {
				ImGui::Text("%-15s", "Main Write");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Aux Write");
			}
			else {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Main Write");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Aux Write");
			}
			if (Memory_state & RAM_SLOTCX_ROM) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Internal Rom");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Cx Rom");
			}
			else {
				ImGui::Text("%-15s", "Internal Rom");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Cx Rom");
			}
			if (Memory_state & RAM_ALT_ZERO_PAGE) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Zero Page");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Alt zp");
			}
			else {
				ImGui::Text("%-15s", "Zero Page");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Alt zp");
			}
			if (Memory_state & RAM_SLOTC3_ROM) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Internal Rom");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "C3 Rom");
			}
			else {
				ImGui::Text("%-15s", "Internal Rom");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "C3 Rom");
			}
			if (Memory_state & RAM_80STORE) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Normal");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "80Store");
			}
			else {
				ImGui::Text("%-15s", "Normal");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "80Store");
			}
			if (Video_mode & VIDEO_MODE_TEXT) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Graphics");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Text");
			}
			else {
				ImGui::Text("%-15s", "Graphics");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Text");
			}
			if (Video_mode & VIDEO_MODE_MIXED) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Not Mixed");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Mixed");
			}
			else {
				ImGui::Text("%-15s", "Not Mixed");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Mixed");
			}
			if (Video_mode & VIDEO_MODE_PAGE2) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Page 1");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Page 2");
			}
			else {
				ImGui::Text("%-15s", "Page 1");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Page 2");
			}
			if (Video_mode & VIDEO_MODE_HIRES) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Lores");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Hires");
			}
			else {
				ImGui::Text("%-15s", "Lores");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Hires");
			}
			if (Video_mode & VIDEO_MODE_ALTCHAR) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Reg char");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "Alt char");
			}
			else {
				ImGui::Text("%-15s", "Reg char");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "Alt char");
			}
			if (Video_mode & VIDEO_MODE_80COL) {
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "40");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%-15s", "80");
			}
			else {
				ImGui::Text("%-15s", "40");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(style.Colors[ImGuiCol_TextDisabled], "%-15s", "80");
			}
		}
	}

	ImGui::End();
}

void debugger_disasm::set_break_addr(uint16_t addr)
{
	m_current_addr = m_break_addr = addr;

	// when setting the breakpoint address, disassemble
	// from that address backwards a "fair distance"
	// and then make sure that all opcodes for the
	// memory locations are filled in to make disassmebly
	// scrolling better.  Due to branches, it's possible
	// for opcode table not to be fully filled in resulting
	// in uneven scrolling.  This code is an attempt to
	// minimize that issue
	static const int num_opcodes = 25;
	uint16_t a = 0xffff;

	for (auto i = num_opcodes; i >= 0; i--) {
		a = memory_find_previous_opcode_addr(m_current_addr, num_opcodes);
		if (a != 0xffff) {
			break;
		}
	}
	SDL_assert(a != 0xffff);

	// starting at the address, disassembly to the current breakpoint
	// address, reading the opcode from memory to fill in
	// the opcode table
	while (a < m_current_addr) {
		cpu_6502::opcode_info *opcode = cpu.get_opcode(memory_read(a, true));
		if (opcode->m_size) {
			a += opcode->m_size;
		} else {
			a++;
		}
	}

	// check to see if we've accurately done the work!
	SDL_assert(a == m_current_addr);
}

void debugger_disasm::attach_console(debugger_console *console)
{
	m_console = console;
}

