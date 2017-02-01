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
#include "debugger_disasm.h"
#include "memory.h"
#include "imgui.h"

const char *debugger_disasm::m_addressing_format_string[] = {
	"",           // NO_MODE
	"",           // ACCUMULATOR_MODE
	"#$%02X",     // IMMEDIATE_MODE
	"",           // IMPLIED_MODE
	"$%02X",      // RELATIVE_MODE
	"$%04X  ",    // ABSOLUTE_MODE
	"$%02X    ",  // ZERO_PAGE_MODE
	"$(%04X)",    // INDIRECT_MODE
	"$%04X,X",    // X_INDEXED_MODE
	"$%04X,Y",    // Y_INDEXED_MODE
	"$%02X,X",    // ZERO_PAGES_INDEXED_MODE
	"$%02X,Y",    // ZERO_PAGED_INDEXED_MODE_Y
	"($%02X,X)",  // INDEXED_INDIRECT_MODE
	"$(%02X),Y",  // INDIRECT_INDEXED_MODE
};

debugger_disasm::debugger_disasm()
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
					sprintf(internal_buffer, "\t$%04X", addressing_val);
					strcat(m_disassembly_line, internal_buffer);
					if (((addressing_val >> 8) & 0xff) != 0xc0) {
						mem_value = memory_read(addressing_val);
						sprintf(internal_buffer, ": %02X", mem_value);
						strcat(m_disassembly_line, internal_buffer);
						if (isprint(mem_value)) {
							sprintf(internal_buffer, " (%c)", mem_value);
							strcat(m_disassembly_line, internal_buffer);
						}
					}
				}
				break;

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
					sprintf(internal_buffer, ": %02X", mem_value);
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

void debugger_disasm::draw(const char *title, uint16_t pc)
{
	if (pc != m_break_addr) {
		m_break_addr = m_current_addr = pc;
	}

	// get the styling so we can get access to color values
	ImGuiStyle &style = ImGui::GetStyle();
	if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_ShowBorders |
				ImGuiWindowFlags_NoScrollbar)) {
		ImGui::Columns(2);
		ImGui::SetColumnOffset(1, ImGui::GetWindowContentRegionWidth() - 120.0f);

		// if window is focued, processes arrow keys to move disassembly
		if (ImGui::IsWindowFocused()) {
			uint16_t new_addr = 0xffff;
			if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
				new_addr = memory_find_previous_opcode_addr(m_current_addr, 1);
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
				// get opcode size and move to new address
				cpu_6502::opcode_info *opcode = cpu.get_opcode(memory_read(m_current_addr));
				if (cpu.get_opcode(memory_read(m_current_addr + opcode->m_size))->m_size > 0) {
					new_addr = m_current_addr + opcode->m_size;
				}
			}
			// only assign the new address if it is valid
			if (new_addr != 0xffff) {
				m_current_addr = new_addr;
			}
		}

		float line_height = ImGui::GetTextLineHeightWithSpacing();
		int line_total_count = 0xffff;
		ImGuiListClipper clipper(line_total_count, line_height);

		// scan backwards in memory until we find the opcode
		// that will put current address in the middle of the screen
		int middle_line = (clipper.DisplayEnd - clipper.DisplayStart) / 2;
		auto addr = memory_find_previous_opcode_addr(m_current_addr, middle_line);

		for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) {
			auto size = get_disassembly(addr);

			// determine color to use
			if (addr == m_break_addr) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
			} else if (addr == m_current_addr) {
				ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text]);
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
			}
			ImGui::Text("%s", m_disassembly_line);
			ImGui::PopStyleColor();
			addr += size;
		}
		clipper.End();

		ImGui::NextColumn();
		ImGui::Text("A  = $%02X", cpu.get_acc());
		ImGui::Text("X  = $%02X", cpu.get_x());
		ImGui::Text("Y  = $%02X", cpu.get_y());
		ImGui::Text("PC = $%04X", cpu.get_pc());
		ImGui::Text("SP = $%04X", cpu.get_sp() + 0x100);

		ImGui::NewLine();
		ImGui::NewLine();
		uint8_t status = cpu.get_status();
		ImGui::Text("%c %c %c %c %c %c %c %c",
			(status >> 7) & 1 ? 'S' : 's',
			(status >> 6) & 1 ? 'V' : 'v',
			(status >> 5) & 1 ? '1' : '0',
			(status >> 4) & 1 ? 'B' : 'b',
			(status >> 3) & 1 ? 'D' : 'd',
			(status >> 2) & 1 ? 'I' : 'i',
			(status >> 1) & 1 ? 'Z' : 'z',
			(status >> 0) & 1 ? 'C' : 'c');
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
	ASSERT(a != 0xffff);

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
	ASSERT(a == m_current_addr);
}

