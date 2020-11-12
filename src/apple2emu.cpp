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

// Emulator.cpp : Defines the entry point for the console application.
//
#include <algorithm>
#include <string>
#include <stdlib.h>
#include <errno.h>

#include "apple2emu_defs.h"
#include "6502.h"
#include "z80softcard.h"
#include "assemble.h"
#include "video.h"
#include "memory.h"
#include "disk.h"
#include "keyboard.h"
#include "joystick.h"
#include "speaker.h"
#include "debugger.h"
#include "path_utils.h"
#include "interface.h"
#include "apple2emu.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"

#include <SDL.h>
#include <SDL_assert.h>

#if defined(_MSC_VER)
#pragma warning(disable:4996)   // disable the deprecated warnings for fopen
#endif

//INITIALIZE_EASYLOGGINGPP

const char *Emulator_names[static_cast<uint8_t>(emulator_type::NUM_EMULATOR_TYPES)] = {
	"Apple ][",
	"Apple ][+",
	"Apple ][e",
	"Apple ][e Enhanced",
};

// globals used to control emulator
uint32_t Speed_multiplier = 1;
bool Auto_start = false;
emulator_state Emulator_state = emulator_state::SPLASH_SCREEN;
emulator_type Emulator_type = emulator_type::APPLE2;

static const char *Disk_image_filename = nullptr;
static const char *Binary_image_filename = nullptr;
static int32_t Program_start_addr = -1;
static int32_t Program_load_addr = -1;

static const char *Log_filename = nullptr;
static FILE *Log_file = nullptr;

uint32_t Total_cycles, Total_cycles_this_frame;

cpu_6502 cpu;
Z80_STATE z80_cpu;

static char *get_cmdline_option(char **start, char **end, const std::string &short_option, const std::string &long_option = "")
{
	char **iter = std::find(start, end, short_option);
	if (iter == end) {
		iter = std::find(start, end, long_option);
	}
	if ((iter != end) && (iter++ != end)) {
		return *iter;
	}

	return nullptr;
}

static bool cmdline_option_exists(char **start, char **end, const std::string &short_option, const std::string &long_option = "")
{
	char **iter = std::find(start, end, short_option);
	if (iter == end) {
		iter = std::find(start, end, long_option);
	}
	if (iter == end) {
		return false;
	}
	return true;
}

// log output function.  Redirected from SDL so that we can do whatever
// we need to with the log message (i.e. stdout, or logfile, etc
static void log_output(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	UNREFERENCED(userdata);
	UNREFERENCED(category);
	UNREFERENCED(priority);

	if (Log_file != nullptr) {
		fprintf(Log_file, "%s\n", message);
	} else {
		fprintf(stderr, "%s\n", message);
	}
}

static void configure_logging()
{
	SDL_LogSetOutputFunction(log_output, nullptr);
}

bool dissemble_6502(const char *filename)
{
	UNREFERENCED(filename);
	return true;
}

void reset_machine()
{
	cpu_6502::cpu_mode mode;

	// ui_init() will load up settings.  Need this before
	// we set the opcodes we need

	if (Emulator_type == emulator_type::APPLE2E_ENHANCED) {
		mode = cpu_6502::cpu_mode::CPU_65C02;
	} else {
		mode = cpu_6502::cpu_mode::CPU_6502;
	}
	memory_init();
	cpu.init(mode);
	z80softcard_init();
	debugger_init();
	speaker_init();
	keyboard_init();
	joystick_init();
	disk_init();
	video_init();

	z80softcard_reset(&z80_cpu);

	if (Binary_image_filename != nullptr && Program_start_addr != -1) {
		FILE *fp = fopen(Binary_image_filename, "rb");
		if (fp == nullptr) {
			printf("Unable to open %s for reading: %d\n", Binary_image_filename, errno);
			exit(-1);
		}
		fseek(fp, 0, SEEK_END);
		auto buffer_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		SDL_assert(buffer_size <= UINT16_MAX);

		// allocate the memory buffer
		uint8_t *buffer = new uint8_t[buffer_size];
		if (buffer == nullptr) {
			printf("Unable to allocate %ld bytes for source code file buffer\n", buffer_size);
			fclose(fp);
			exit(-1);
		}
		fread(buffer, 1, buffer_size, fp);
		fclose(fp);
		memory_load_buffer(buffer, (uint16_t)buffer_size, (uint16_t)Program_load_addr);
		cpu.set_pc((uint16_t)Program_start_addr);
		Emulator_state = emulator_state::EMULATOR_TEST;
		debugger_enter();
	}
}

static void apple2emu_shutdown()
{
	if (Log_file != nullptr) {
		fclose(Log_file);
	}

	ui_shutdown();
	video_shutdown();
	disk_shutdown();
	keyboard_shutdown();
	joystick_shutdown();
	debugger_shutdown();
	memory_shutdown();

	SDL_Quit();
}

int main(int argc, char* argv[])
{
	// grab some needed command line options
	Disk_image_filename = get_cmdline_option(argv, argv + argc, "-d", "--disk");
	Binary_image_filename = get_cmdline_option(argv, argv + argc, "-b", "--binary");

	const char *addr_string = get_cmdline_option(argv, argv + argc, "-p", "--pc");
	if (addr_string != nullptr) {
		Program_start_addr = (uint16_t)strtol(addr_string, nullptr, 16);
	}

	addr_string = get_cmdline_option(argv, argv + argc, "-b", "--base");
	if (addr_string != nullptr) {
		Program_load_addr = (uint16_t)strtol(addr_string, nullptr, 16);
	}

	bool test_z80 = cmdline_option_exists(argv, argv + argc, "-z", "--z80");
	Log_filename = get_cmdline_option(argv, argv + argc, "-l", "--log");
	if (Log_filename != nullptr) {
		Log_file = fopen(Log_filename, "wt");
	}

	// set up atexit handler to clean everything up
	atexit(apple2emu_shutdown);

	// initialize SDL before everything else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return -1;
	}
	configure_logging();
	ui_init();
	reset_machine();

	if (test_z80) {
		SDL_Quit();
		Emulator_type = emulator_type::APPLE2;
		memory_init_for_z80_test();
		z80_cpu.status = 0;
		z80_cpu.pc = Program_start_addr;
		while (true) {
			z80softcard_emulate(&z80_cpu, 0);
		}
		exit(0);
	}


	if (Auto_start == true) {
		Emulator_state = emulator_state::EMULATOR_STARTED;
	}

	bool quit = false;
	Total_cycles_this_frame = Total_cycles = 0;

	while (!quit) {
		uint32_t cycles_per_frame = Cycles_per_frame * Speed_multiplier;  // we can speed up machine by multiplier here


		// process the next opcode
		if (Emulator_state == emulator_state::EMULATOR_STARTED ||
			Emulator_state == emulator_state::EMULATOR_TEST) {
			speaker_unpause();
			while (true) {
				// process debugger (before opcode processing so that we can break on
				// specific addresses properly
				bool next_statement = debugger_process();

				if (next_statement) {
					// note that we might emulate z80 or 6502 here
					uint32_t cycles = z80softcard_emulate(&z80_cpu, 0);
					if (cycles == 0) {
						cycles = cpu.process_opcode();
					}
					Total_cycles_this_frame += cycles;
					Total_cycles += cycles;

					// update speaker if needed
					speaker_update(cycles);

					if (Total_cycles_this_frame > cycles_per_frame) {
						// this is essentially number of cycles for one redraw cycle
						// for TV/monitor.  Around 17030 cycles I believe
						Total_cycles_this_frame -= cycles_per_frame;
						break;
					}
				} else {
					break;
				}
			}
		} else {
			debugger_process();
		}

		SDL_Event evt;
		while (SDL_PollEvent(&evt)) {
			ImGui_ImplSDL2_ProcessEvent(&evt);

			// process events.  Keypressed will be processed through
			// imgui in the interface code
			switch (evt.type) {
			case SDL_WINDOWEVENT:
				if (evt.window.event == SDL_WINDOWEVENT_CLOSE) {
					quit = true;
				}
				break;

			case SDL_QUIT:
				quit = true;
				break;
			}
		}

		speaker_queue_audio();
		ui_do_frame();
	}

	return 0;
}

