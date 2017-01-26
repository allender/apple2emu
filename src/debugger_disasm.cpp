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
#include "debugger_disasm.h"
#include "memory.h"
#include "imgui.h"

char *debugger_disasm::m_addressing_format_string[] = {
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
	cpu_6502::opcode_info *opcode = &cpu_6502::m_opcodes[memory_read(addr)];
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
					mem_value = memory_read(addressing_val);
					sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
					strcat(m_disassembly_line, internal_buffer);
					if (isprint(mem_value)) {
						sprintf(internal_buffer, " (%c)", mem_value);
						strcat(m_disassembly_line, internal_buffer);
					}
				}
				break;

				// indexed and zero page indexed are the same
			case cpu_6502::addr_mode::X_INDEXED_MODE:
			case cpu_6502::addr_mode::ZP_INDEXED_MODE:
				addressing_val += cpu.get_x();
				mem_value = memory_read(addressing_val);
				sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
				strcat(m_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(m_disassembly_line, internal_buffer);
				}
				break;

				// same as x-indexed mode
			case cpu_6502::addr_mode::Y_INDEXED_MODE:
			case cpu_6502::addr_mode::ZP_INDEXED_MODE_Y:
				addressing_val += cpu.get_y();
				mem_value = memory_read(addressing_val);
				sprintf(internal_buffer, "\t%04X: %02X", addressing_val, mem_value);
				strcat(m_disassembly_line, internal_buffer);
				if (isprint(mem_value)) {
					sprintf(internal_buffer, " (%c)", mem_value);
					strcat(m_disassembly_line, internal_buffer);
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

void debugger_disasm::draw(const char *title)
{
	static bool column_set = false;
	auto cur_addr = cpu.get_pc();
	if (ImGui::Begin(title, nullptr, 0)) {
		ImGui::Columns(2);
		if (column_set == false) {
			ImGui::SetColumnOffset(1, ImGui::GetColumnOffset(1) + 100.0f);
			column_set = true;
		}

		ImGui::BeginChild("##scrolling", ImVec2(0, 0));

		float line_height = ImGui::GetTextLineHeight();
		int line_total_count = 0xffff;
		ImGuiListClipper clipper(line_total_count, line_height);

		// scan backwards in memory until we find the opcode
		// that will put current address in the middle of the screen
		int middle_line = (clipper.DisplayEnd - clipper.DisplayStart) / 2;
		auto addr = memory_find_previous_opcode_addr(cur_addr, middle_line);

		bool sanity_check = false;
		for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) {
			if (addr > cur_addr && sanity_check == false) {
				ASSERT(0);
			}
			if (addr == cur_addr) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
				sanity_check = true;
			}
			auto size = get_disassembly(addr);
			ImGui::Text("%s", m_disassembly_line);
			if (addr == cur_addr) {
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

