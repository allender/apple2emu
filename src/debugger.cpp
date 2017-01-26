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
#include "debugger_console.h"
#include "debugger_memory.h"
#include "debugger_disasm.h"

debugger_state Debugger_state = debugger_state::IDLE;
std::vector<breakpoint> Debugger_breakpoints;

static const uint32_t Debugger_status_line_length = 256;
static char Debugger_status_line[Debugger_status_line_length];

static FILE *Debugger_trace_fp = nullptr;

static ImGuiWindowFlags default_window_flags = ImGuiWindowFlags_ShowBorders;

// definitions for windows
static debugger_console Debugger_console;
static debugger_memory_editor Debugger_memory_editor;
static debugger_disasm Debugger_disasm;

// lambdas for console commands for debugger
auto step_command = [](char *) { Debugger_state = debugger_state::SINGLE_STEP; };
auto continue_command = [](char *) { Debugger_state = debugger_state::SHOW_ALL; };
auto stop_command = [](char *) { Debugger_state = debugger_state::SINGLE_STEP; };
auto exit_command = [](char *) { Debugger_state = debugger_state::IDLE; };

auto quit_command = [](char *) {
	exit(-1);
};

auto break_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		uint16_t address = (uint16_t)strtol(token, nullptr, 16);
		breakpoint b;

		b.m_type = breakpoint_type::BREAKPOINT;
		b.m_addr = address;
		b.m_enabled = true;
		Debugger_breakpoints.push_back(b);
	}
};

auto enable_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints[val].m_enabled = true;
			Debugger_console.add_log("Breakpoint %lu enabled.", val);
		} else {
			Debugger_console.add_log("%lu is not a valid index into breakpoint list.", val);
		}
	}
};

auto disable_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints[val].m_enabled = false;
			Debugger_console.add_log("Breakpoint %lu disabled.", val);
		} else {
			Debugger_console.add_log("%lu is not a valid index into breakpoint list.", val);
		}
	}
};

auto list_command = [](char *) {
	int index = 0;
	if (Debugger_breakpoints.size() == 0) {
		Debugger_console.add_log("No breakpoints defined.");
	} else {
		for (auto bp : Debugger_breakpoints) {
			if (bp.m_enabled) {
				Debugger_console.add_log("%d - 0x%04x enabled", index++, bp.m_addr);
			} else {
				Debugger_console.add_log("%d - 0x%04x disabled", index++, bp.m_addr);
			}
		}
	}
};

auto delete_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (token != nullptr) {
		size_t val = strtol(token, nullptr, 10);
		if (val < Debugger_breakpoints.size()) {
			Debugger_breakpoints.erase(Debugger_breakpoints.begin() + val);
			Debugger_console.add_log("Breakpoint %lu deleted.", val);
		} else {
			Debugger_console.add_log("%lu not a valid index into breakpoint list.", val);
		}
	}
};

auto trace_command = [](char *) {
	char *token = strtok(nullptr, " ");
	if (Debugger_trace_fp == nullptr) {
		const char *f = "trace.log";

		if (token != nullptr) {
			f = token;
		}

		Debugger_trace_fp = fopen(f, "wt");
	} else {
		if (Debugger_trace_fp != nullptr) {
			fclose(Debugger_trace_fp);
			Debugger_trace_fp = nullptr;
		}
	}
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

static void debugger_trace_line()
{
	if (Debugger_trace_fp == nullptr) {
		return;
	}

	// print out the info the trace file
	debugger_get_short_status();
	Debugger_disasm.get_disassembly(cpu.get_pc());
	//fprintf(Debugger_trace_fp, "%s  %s\n", Debugger_status_line, Debugger_disassembly_line);
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

	// add in the debugger commands
	Debugger_console.add_command("step", "Single step assembly", step_command);
	Debugger_console.add_command("continue",
		"Continue execution in debugger", continue_command);
	Debugger_console.add_command("stop",
		"Stop execution when in debugger", stop_command);
	Debugger_console.add_command("exit", "Exit debugger", exit_command);
	Debugger_console.add_command("quit", "Quit emulator", quit_command);
	Debugger_console.add_command("break",
		"Create breakpoint (break <addr>)", break_command);
	Debugger_console.add_command("enable",
		"Enable breakpoint (enable <bp#>)", enable_command);
	Debugger_console.add_command("disable",
		"Disable breakpoint (disable <bp#>)", disable_command);
	Debugger_console.add_command("list",
		"List breakpoints", list_command);
	Debugger_console.add_command("delete",
		"Delete breakpoint (delete bp#)", delete_command);
	Debugger_console.add_command("trace",
		"Toggle trace to file.  (trace <filename> - filename"
		" is optional and if not specified, output will be to 'trace.log'",
		trace_command);
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
	Debugger_memory_editor.draw("Memory", 0x10000, 0);
	Debugger_disasm.draw("Disassembly");
	Debugger_console.draw("Console", nullptr);
	debugger_display_breakpoints();
	debugger_display_soft_switch();
}
