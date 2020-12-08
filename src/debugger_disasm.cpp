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
#include <fstream>
#include <regex>

#include "SDL_scancode.h"
#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "debugger.h"
#include "debugger_disasm.h"
#include "imgui.h"
#include "keyboard.h"
#include "memory.h"
#include "video.h"


static std::regex Symtable_regex("\\s*[0x$]*([0-9A-Fa-f]{2,4})\\s+(\\w{1,8}).*");

// these string formats need to match the addressing modes
// defined in 6502.h
const char *debugger_disasm::m_addressing_format_string[] = {
	"",           // NO_MODE
	"",           // ACCUMULATOR_MODE
	"#%02X",      // IMMEDIATE_MODE
	"",           // IMPLIED_MODE
	"$%04X",      // RELATIVE_MODE
	"$%04X",      // ABSOLUTE_MODE
	"$%02X",      // ZERO_PAGE_MODE
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

const char *debugger_disasm::m_addressing_format_string_with_symbol[] = {
	"",                  // NO_MODE
	"",                  // ACCUMULATOR_MODE
	"#%02X",             // IMMEDIATE_MODE
	"",                  // IMPLIED_MODE
	"%-8s  ($%04X)",     // RELATIVE_MODE
	"%-8s  ($%04X)",     // ABSOLUTE_MODE
	"%-8s  ($%02X)",     // ZERO_PAGE_MODE
	"(%s)  ($%04X)",     // INDIRECT_MODE
	"(%s)  ($%02X)",     // INDIRECT_ZP_MODE
	"%-8s,X  ($%04X)",   // X_INDEXED_MODE
	"%-8s,Y  ($%04X)",   // Y_INDEXED_MODE
	"%-8s,X  ($%02X)",   // ZERO_PAGES_INDEXED_MODE
	"%-8s,Y  ($%02X)",   // ZERO_PAGED_INDEXED_MODE_Y
	"(%s,X)  ($%02X)",   // INDEXED_INDIRECT_MODE'
	"",                  // ABSOLUTE_INDEXED_INDIRECT_MODE
	"(%s),Y  ($%02X)",   // INDIRECT_INDEXED_MODE
};

// loads a symbol table from the given filename.  We expect
// symbol tables to have a strict format
// <addr> <symbol>
// Any other lines will be ignored
// address is assumed bo be hexidecimal
// symbols will be truncated to 8 characters (to fit better in disasm window)
void debugger_disasm::add_symtable(const std::string &filename)
{
	std::smatch match;
	std::ifstream infile(filename);
	std::string line;

	while (std::getline(infile, line)) {
		if (std::regex_match(line, match, Symtable_regex) == true) {
			std::string addr_str = match[1];
			std::string name = match[2];
			uint16_t addr = (uint16_t)strtol(addr_str.c_str(), nullptr, 16);
			m_symtable[addr] = name;
		}
	}
}

void debugger_disasm::remove_symtable(const std::string &filename)
{
	std::smatch match;
	std::ifstream infile(filename);
	std::string line;

	while (std::getline(infile, line)) {
		if (std::regex_match(line, match, Symtable_regex) == true) {
			std::string addr_str = match[1];
			std::string name = match[2];
			uint16_t addr = (uint16_t)strtol(addr_str.c_str(), nullptr, 16);
			m_symtable.erase(addr);
		}
	}
}

const char *debugger_disasm::find_symbol(uint16_t addr)
{
	const char *s = nullptr;
	auto element = m_symtable.find(addr);
	if (element != m_symtable.end()) {
		s = element->second.c_str();
	}
	return s;
}

debugger_disasm::debugger_disasm() : m_console(nullptr),
									 m_reset_window(false)
{
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
		const char *addr_symbol;
		if (m_symtable.empty() == false) {
			addr_symbol = find_symbol(addr);
			if (addr_symbol != nullptr) {
				sprintf(internal_buffer, "%8s ", addr_symbol);
			} else {
				sprintf(internal_buffer, "         ");
			} 
			strcat(m_disassembly_line, internal_buffer);
		}

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
		sprintf(internal_buffer, " %c%c%c ", opcode->m_mnemonic >> 24,
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
			addr_symbol = find_symbol(addressing_val);
			if (addr_symbol == nullptr || mode == cpu_6502::addr_mode::IMMEDIATE_MODE) {
				sprintf(internal_buffer, m_addressing_format_string[static_cast<uint8_t>(mode)], addressing_val);
			} else {
				sprintf(internal_buffer, m_addressing_format_string_with_symbol[static_cast<uint8_t>(mode)], addr_symbol, addressing_val);
			}
			strcat(m_disassembly_line, internal_buffer);
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

	ImGui::SetNextWindowSize(ImVec2(546, 756), condition);
	ImGui::SetNextWindowPos(ImVec2(570, 2), condition);

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
				} else if (ImGui::IsKeyPressed(SDL_SCANCODE_F11)) {
					// step over (or next)
					m_console->execute_command(step_command_name);
				} else if (ImGui::IsKeyPressed(SDL_SCANCODE_F10)) {
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
		ImGui::SetCursorPosY(5.0f);
		ImGui::NewLine();
		if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
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
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Soft Switches", ImGuiTreeNodeFlags_CollapsingHeader)) {
			if (Memory_state & RAM_CARD_BANK2) { ImGui::Text("%-15s", "Bank1");
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

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Breakpoints", ImGuiTreeNodeFlags_CollapsingHeader)) {
			// for now, just disassm from the current pc
			for (size_t i = 0; i < Debugger_breakpoints.size(); i++) {

				// don't display temporary breakpoints as these are used
				// for step over
				if (Debugger_breakpoints[i].m_type == breakpoint_type::TEMPORARY) {
					continue;
				}
				ImGui::Text("%-3lu", i);
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

				case breakpoint_type::TEMPORARY:
				case breakpoint_type::INVALID:
					break;
				}

				ImGui::SameLine();
				ImGui::Text("$%-6x", Debugger_breakpoints[i].m_addr);
				ImGui::SameLine();
				ImGui::Text("%s", Debugger_breakpoints[i].m_enabled == true ? "enabled" : "disabled");
			}
		}

		// input for "goto".   use 5 buyes, which limits input
		// to a max of 4 hexidecimal characters
		static char goto_addr[5] = "";
		ImGui::NewLine();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.20f);
		ImGui::InputText("", goto_addr, 5, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
		ImGui::SameLine();
		if (ImGui::Button("Goto") == true) {
			if (strlen(goto_addr) > 0) {
				// goto to the address in the input text box
				m_current_addr = (uint16_t)strtol(goto_addr, nullptr, 16);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("PC") == true) {
			m_current_addr = pc; 
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
