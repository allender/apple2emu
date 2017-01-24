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

static const uint32_t Max_input = 256;
static const uint32_t Max_breakpoints = 15;

static const uint32_t Debugger_status_line_length = 256;
static const uint32_t Debugger_disassembly_line_length = 256;
static char Debugger_input[Max_input];
static char Debugger_status_line[Debugger_status_line_length];
static char Debugger_disassembly_line[Debugger_disassembly_line_length];

static debugger_state Debugger_state = debugger_state::IDLE;
static bool Debugger_trace = false;
static FILE *Debugger_trace_fp = nullptr;

static breakpoint Debugger_breakpoints[Max_breakpoints];
static int Debugger_num_breakpoints;

static ImGuiWindowFlags default_window_flags = ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

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
	sprintf(Debugger_status_line, "%02x %02X %02X %04X %c%c%c%c%c%c%c%c", cpu.get_acc(), cpu.get_x(), cpu.get_y(), cpu.get_sp() + 0x100,
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
	sprintf(Debugger_status_line, "%04X   A=%02x   X=%02X   Y=%02X  Status = %c%c%c%c%c%c%c%c", cpu.get_pc(), cpu.get_acc(), cpu.get_x(), cpu.get_y(),
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

// disassemble at the given address
static void debugger_disassemble(uint16_t addr, int32_t num_lines = 20)
{
	for (auto i = 0; i < num_lines; i++) {
		uint8_t size = debugger_get_disassembly(addr);
		addr += size;
	}
}

static void debugger_stop_trace()
{
	if (Debugger_trace_fp != nullptr) {
		fclose(Debugger_trace_fp);
	}
}

static void debugger_start_trace()
{
	// stop any trace currently running
	debugger_stop_trace();

	Debugger_trace_fp = fopen("trace.log", "wt");
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

static void debugger_process_commands()
{
	debugger_get_status();
	debugger_get_disassembly(cpu.get_pc());

	// parse the debugger commands
	char *token = strtok(Debugger_input, " ");
	if (token == nullptr) {
		return;
	}

	// step to next statement
	if (!stricmp(token, "s") || !stricmp(token, "step")) {
		Debugger_state = debugger_state::SINGLE_STEP;
	}
	else if (!stricmp(token, "c") || !stricmp(token, "cont")) {
		Debugger_state = debugger_state::SHOW_ALL;
	}

	else if (!stricmp(token, "b") || !stricmp(token, "break") ||
		!stricmp(token, "rw") || !stricmp(token, "rwatch") ||
		!stricmp(token, "ww") || !stricmp(token, "wwatch")) {
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

		if (Debugger_num_breakpoints < Max_breakpoints) {
			token = strtok(nullptr, " ");
			if (token != nullptr) {
				uint16_t address = (uint16_t)strtol(token, nullptr, 16);
				if (address > 0xffff) {
				}
				else {
					Debugger_breakpoints[Debugger_num_breakpoints].m_type = bp_type;
					Debugger_breakpoints[Debugger_num_breakpoints].m_addr = address;
					Debugger_breakpoints[Debugger_num_breakpoints].m_enabled = true;
					Debugger_num_breakpoints++;
				}
			}
		}
	}

	// enable and disable breakpoints
	else if (!stricmp(token, "enable") || !stricmp(token, "disable")) {
		bool enable = !stricmp(token, "enable");
		token = strtok(nullptr, " ");
		if (token == nullptr) {
			// enable all breakpoints
			for (auto i = 0; i < Debugger_num_breakpoints; i++) {
				Debugger_breakpoints[i].m_enabled = enable;
			}
		}
		else {
			int i = atoi(token);
			if (i >= 0 && i < Debugger_num_breakpoints) {
				Debugger_breakpoints[i].m_enabled = enable;
			}
		}
	}

	// delete breakpoints
	else if (!stricmp(token, "del")) {
		token = strtok(nullptr, " ");
		if (token == nullptr) {
			// delete all  breakpoints
			Debugger_num_breakpoints = 0;
		}
		else {
			int i = atoi(token);
			if (i >= 0 && i < Debugger_num_breakpoints) {
				for (auto j = i + 1; j < Debugger_num_breakpoints; j++) {
					Debugger_breakpoints[j - 1] = Debugger_breakpoints[j];
				}
				Debugger_num_breakpoints--;
			}
		}
	}

	// disassemble.  With no address, use current PC
	// if address given, then disassemble at that adddress
	else if (!stricmp(token, "dis")) {
		uint16_t addr;

		token = strtok(nullptr, " ");
		if (token == nullptr) {
			addr = cpu.get_pc();
		}
		else {
			addr = (uint16_t)strtol(token, nullptr, 16);
		}
		debugger_disassemble(addr);
	}

	// quit
	else if (!stricmp(token, "q") || !stricmp(token, "quit")) {
		debugger_shutdown();  // clears out curses changes to console
		exit(-1);
	}

	// stop/start trace
	else if (!stricmp(token, "trace")) {

		Debugger_trace = !Debugger_trace;
		if (Debugger_trace == true) {
			debugger_start_trace();
		}
		else {
			debugger_stop_trace();
		}

	}
}

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
			ImVec2 memory_pos(0.0f, (float)(Video_window_size.h) / 2.0f + 5.0f);
			ImVec2 memory_size((float)(Video_window_size.w / 2.0f) - 80.0f, (float)Video_window_size.h / 2.0f - 5.0f);
			ImGui::SetWindowPos(memory_pos);
			ImGui::SetWindowSize(memory_size);

			ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

			int addr_digits_count = 0;
			for (int n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
				addr_digits_count++;

			float glyph_width = ImGui::CalcTextSize("F").x;
			float cell_width = glyph_width * 3; // "FF " we include trailing space in the width to easily catch clicks everywhere

			float line_height = ImGui::GetTextLineHeight();
			int line_total_count = (int)((mem_size + Rows-1) / Rows);
			ImGuiListClipper clipper(line_total_count, line_height);
			int visible_start_addr = clipper.DisplayStart * Rows;
			int visible_end_addr = clipper.DisplayEnd * Rows;

			bool data_next = false;

			if (!AllowEdits || DataEditingAddr >= mem_size)
				DataEditingAddr = -1;

			int data_editing_addr_backup = DataEditingAddr;
			if (DataEditingAddr != -1)
			{
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= Rows)                   { DataEditingAddr -= Rows; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Rows)  { DataEditingAddr += Rows; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0)                { DataEditingAddr -= 1; DataEditingTakeFocus = true; }
				else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1)    { DataEditingAddr += 1; DataEditingTakeFocus = true; }
			}
			if ((DataEditingAddr / Rows) != (data_editing_addr_backup / Rows))
			{
				// Track cursor movements
				float scroll_offset = ((DataEditingAddr / Rows) - (data_editing_addr_backup / Rows)) * line_height;
				bool scroll_desired = (scroll_offset < 0.0f && DataEditingAddr < visible_start_addr + Rows*2) || (scroll_offset > 0.0f && DataEditingAddr > visible_end_addr - Rows*2);
				if (scroll_desired)
					ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset);
			}

			bool draw_separator = true;
			for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible items
			{
				uint16_t addr = (uint16_t)(line_i * Rows);
				ImGui::Text("%0*lX: ", addr_digits_count, base_display_addr+addr);
				ImGui::SameLine();

				// Draw Hexadecimal
				float line_start_x = ImGui::GetCursorPosX();
				for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
				{
					ImGui::SameLine(line_start_x + cell_width * n);

					if (DataEditingAddr == addr)
					{
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
						if (DataEditingTakeFocus)
						{
							ImGui::SetKeyboardFocusHere();
							sprintf(AddrInput, "%0*lX", addr_digits_count, base_display_addr+addr);
							sprintf(DataInput, "%02X", memory_read(addr));
						}
						ImGui::PushItemWidth(ImGui::CalcTextSize("FF").x);
						ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_NoHorizontalScroll|ImGuiInputTextFlags_AlwaysInsertMode|ImGuiInputTextFlags_CallbackAlways;
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
						if (AllowEdits && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
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
					ImGui::GetWindowDrawList()->AddLine(ImVec2(screen_pos.x - glyph_width, screen_pos.y - 9999), ImVec2(screen_pos.x - glyph_width, screen_pos.y + 9999), ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]));
					draw_separator = false;
				}

				// Draw ASCII values
				addr = (uint16_t)(line_i * Rows);
				for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
				{
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

			if (data_next && DataEditingAddr < mem_size)
			{
				DataEditingAddr = DataEditingAddr + 1;
				DataEditingTakeFocus = true;
			}

			ImGui::Separator();

			ImGui::AlignFirstTextHeightToWidgets();
			ImGui::SameLine();
			ImGui::Text("Range %0*X..%0*X", addr_digits_count, (int)base_display_addr, addr_digits_count, (int)base_display_addr+mem_size-1);
			ImGui::SameLine();
			ImGui::PushItemWidth(70);
			if (ImGui::InputText("##addr", AddrInput, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				int goto_addr;
				if (sscanf(AddrInput, "%X", &goto_addr) == 1)
				{
					goto_addr -= base_display_addr;
					if (goto_addr >= 0 && goto_addr < mem_size)
					{
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
         if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
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

static void debugger_display_registers()
{
	ImGuiWindowFlags flags = default_window_flags | ImGuiWindowFlags_NoInputs;
	if (ImGui::Begin("Registers", nullptr, flags)) {
		ImVec2 register_pos((float)(Video_window_size.w/2.0f) + 5.0f, 5.0f);
		ImVec2 register_size((float)(Video_window_size.w/3.0f) , (float)Video_window_size.h/2.0f);
		ImGui::SetWindowPos(register_pos);
		ImGui::SetWindowSize(register_size);

		ImGui::Text("A  = $%02X", cpu.get_acc());
		ImGui::Text("X  = $%02X", cpu.get_x());
		ImGui::Text("Y  = $%02X", cpu.get_y());
		ImGui::Text("PC = $%04X", cpu.get_pc());
		ImGui::Text("SP = $%04X", cpu.get_sp() + 0x100);
	}
	ImGui::End();
}

// display the disassembly in the disassembly window
static void debugger_display_disasm()
{
	auto addr = cpu.get_pc();
	if (ImGui::Begin("Disassembly", nullptr, default_window_flags)) {
		ImVec2 disassembly_pos((float)(Video_window_size.w/2.0f) - 75.0f, (float)(Video_window_size.h) / 2.0f + 5.0f);
		ImVec2 disassembly_size((float)(Video_window_size.w / 3.0f), (float)Video_window_size.h / 2.0f - 5.0f);
		ImGui::SetWindowPos(disassembly_pos);
		ImGui::SetWindowSize(disassembly_size);

		ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

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
		ImGui::PopStyleVar(2);

		ImGui::EndChild();

		ImGui::Separator();

		ImGui::AlignFirstTextHeightToWidgets();
		ImGui::SameLine();
		ImGui::Text("Command: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(250);

		if (ImGui::InputText("##addr", Debugger_input, Max_input, ImGuiInputTextFlags_EnterReturnsTrue)) {
			// process commands here
			debugger_process_commands();
			Debugger_input[0] = '\0';
		}
		ImGui::PopItemWidth();

      // keeps focus on input box
		if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))) {
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
      }
	}
	ImGui::End();

}

// display ll breakpoints
static void debugger_display_breakpoints()
{
#if 0
	wclear(Debugger_breakpoint_window);
	box(Debugger_breakpoint_window, 0, 0);
	// for now, just disassm from the current pc
	for (auto i = 0; i < Debugger_num_breakpoints; i++) {
		wmove(Debugger_breakpoint_window, i + 1, 1);
		wprintw(Debugger_breakpoint_window, "%-3d", i);
		switch (Debugger_breakpoints[i].m_type) {
		case breakpoint_type::BREAKPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Bp");
			break;

		case breakpoint_type::RWATCHPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Rwp");
			break;

		case breakpoint_type::WWATCHPOINT:
			wprintw(Debugger_breakpoint_window, "%-5s", "Wwp");
			break;

		case breakpoint_type::INVALID:
			break;
		}

		wprintw(Debugger_breakpoint_window, "$%-6x", Debugger_breakpoints[i].m_addr);
		wprintw(Debugger_breakpoint_window, "%s", Debugger_breakpoints[i].m_enabled == true ? "1" : "0");

	}
	wrefresh(Debugger_breakpoint_window);
#endif
}

static void debugger_display_status()
{
	ImGuiWindowFlags flags = default_window_flags | ImGuiWindowFlags_NoInputs;
	if (ImGui::Begin("Video/Memory flags", nullptr, flags)) {
		ImVec2 status_pos((float)(Video_window_size.w * 2.0f /3.0f) + 120.0f, (float)(Video_window_size.h) / 2.0f + 5.0f);
		ImVec2 status_size((float)(Video_window_size.w / 3.0f - 125.0f), (float)Video_window_size.h / 2.0f - 5.0f);
		ImGui::SetWindowPos(status_pos);
		ImGui::SetWindowSize(status_size);
		ImGui::Text("$c011: ");
		ImGui::SameLine();
		if (Memory_state & RAM_CARD_BANK2) {
			ImGui::Text("Bank1/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Bank2");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Bank1");
			ImGui::SameLine();
			ImGui::Text("/Bank2");
		}
		ImGui::Text("$c012: ");
		ImGui::SameLine();
		if (Memory_state & RAM_CARD_READ) {
			ImGui::Text("Ram Card Write/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Ram Card Read");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Ram Card Write");
			ImGui::SameLine();
			ImGui::Text("/Ram Card Read");
		}
		ImGui::Text("$c013: ");
		ImGui::SameLine();
		if (Memory_state & RAM_AUX_MEMORY_READ) {
			ImGui::Text("Main Read/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Aux Read");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Main Read");
			ImGui::SameLine();
			ImGui::Text("/Aux Read");
		}
		ImGui::Text("$c014: ");
		ImGui::SameLine();
		if (Memory_state & RAM_AUX_MEMORY_WRITE) {
			ImGui::Text("Main Write/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Aux Write");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Main Write");
			ImGui::SameLine();
			ImGui::Text("/Aux Write");
		}
		ImGui::Text("$c015: ");
		ImGui::SameLine();
		if (Memory_state & RAM_SLOTCX_ROM) {
			ImGui::Text("Internal Rom/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Cx Rom");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Internal Rom");
			ImGui::SameLine();
			ImGui::Text("/Cx Rom");
		}
		ImGui::Text("$c016: ");
		ImGui::SameLine();
		if (Memory_state & RAM_ALT_ZERO_PAGE) {
			ImGui::Text("Zero Page/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Alt zp");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Zero Page");
			ImGui::SameLine();
			ImGui::Text("/Alt zp");
		}
		ImGui::Text("$c017: ");
		ImGui::SameLine();
		if (Memory_state & RAM_SLOTC3_ROM) {
			ImGui::Text("Internal Rom/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "C3 Rom");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Internal Rom");
			ImGui::SameLine();
			ImGui::Text("/C3 Rom");
		}
		ImGui::Text("$c018: ");
		ImGui::SameLine();
		if (Memory_state & RAM_80STORE) {
			ImGui::Text("Normal/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "80Store");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Normal");
			ImGui::SameLine();
			ImGui::Text("/80Store");
		}
		ImGui::Text("$c01a: ");
		ImGui::SameLine();
		if (Video_mode & VIDEO_MODE_TEXT) {
			ImGui::Text("Graphics/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Text");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Graphics");
			ImGui::SameLine();
			ImGui::Text("/Text");
		}
		ImGui::Text("$c01b: ");
		ImGui::SameLine();
		if (Video_mode & VIDEO_MODE_MIXED) {
			ImGui::Text("Not Mixed/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Mixed");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Not Mixed");
			ImGui::SameLine();
			ImGui::Text("/Mixed");
		}
		ImGui::Text("$c01c: ");
		ImGui::SameLine();
		if (!(Video_mode & VIDEO_MODE_PAGE2)) {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Page 1");
			ImGui::SameLine();
			ImGui::Text("/Page 2");
		}
		else {
			ImGui::Text("Page 1/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Page 2");
		}
		ImGui::Text("$c01d: ");
		ImGui::SameLine();
		if (Video_mode & VIDEO_MODE_HIRES) {
			ImGui::Text("Lores/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Hires");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lores");
			ImGui::SameLine();
			ImGui::Text("/Hires");
		}
		ImGui::Text("$c01e: ");
		ImGui::SameLine();
		if (Video_mode & VIDEO_MODE_ALTCHAR) {
			ImGui::Text("Reg char set/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Alt char set");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Reg char set");
			ImGui::SameLine();
			ImGui::Text("/Alt char set");
		}
		ImGui::Text("$c01f: ");
		ImGui::SameLine();
		if (Video_mode & VIDEO_MODE_80COL) {
			ImGui::Text("40/");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "80");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "40");
			ImGui::SameLine();
			ImGui::Text("/80");
		}


	}
	ImGui::End();
}

void debugger_enter()
{
	Debugger_state = debugger_state::WAITING_FOR_INPUT;
}

void debugger_exit()
{
}

void debugger_shutdown()
{
}

void debugger_init()
{
	Debugger_state = debugger_state::IDLE;
	Debugger_num_breakpoints = 0;
}

bool debugger_process()
{
	bool continue_execution = true;

	// check on breakpoints
	if (Debugger_state == debugger_state::IDLE) {
		for (auto i = 0; i < Debugger_num_breakpoints; i++) {
			if (Debugger_breakpoints[i].m_enabled == true) {
				switch (Debugger_breakpoints[i].m_type) {
				case breakpoint_type::BREAKPOINT:
					if (cpu.get_pc() == Debugger_breakpoints[i].m_addr) {
						debugger_enter();
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

	// if we are stopped, process any commands here in tight loop
	//if (Debugger_stopped) {
	//	//debugger_process_commands(cpu);
	//	if (Debugger_stopped == false) {
	//		debugger_exit();
	//	}
	//}

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
	debugger_display_registers();
	debugger_display_disasm();
	debugger_display_breakpoints();
	debugger_display_status();
}
