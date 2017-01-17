/*

MIT License

Copyright (c) 2016 Mark Allender


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

#include "cpu.h"
#include "assemble.h"
#include "video.h"
#include "disk.h"
#include "keyboard.h"
#include "joystick.h"
#include "speaker.h"
#include "debugger.h"
#include "path_utils.h"
#include "interface.h"
#include "apple2emu.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

#include "SDL.h"

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

static SDL_sem *cpu_sem;

// globals used to control emulator
uint32_t Speed_multiplier = 1;
bool Auto_start = false;
emulator_state Emulator_state = emulator_state::SPLASH_SCREEN;
emulator_type Emulator_type = emulator_type::APPLE2;

const char *Disk_image_filename = nullptr;

uint32_t Total_cycles, Total_cycles_this_frame;

cpu_6502 cpu;

static void configure_logging()
{
	//el::Configurations conf;

	//conf.setToDefault();
	//conf.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
	//conf.set(el::Level::Global, el::ConfigurationType::LogFileTruncate, "true");
	//conf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
	//conf.set(el::Level::Global, el::ConfigurationType::Format, "%msg");
	//el::Loggers::reconfigureLogger("default", conf);

	//// set some options on the logger
	//el::Loggers::addFlag(el::LoggingFlag::ImmediateFlush);
	//el::Loggers::addFlag(el::LoggingFlag::NewLineForContainer);
	//el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
}


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

#if 0
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
#endif

static uint32_t render_event_timer(uint32_t interval, void *param)
{
	UNREFERENCED(param);
	SDL_SemPost(cpu_sem);
	return interval;
}

bool dissemble_6502(const char *filename)
{
	UNREFERENCED(filename);
	return true;
}

void reset_machine()
{
	ui_init();
	memory_init();
	cpu.init();
	debugger_init();
	speaker_init();
	keyboard_init();
	joystick_init();
	disk_init();
	video_init();
}

int main(int argc, char* argv[])
{
	// configure logging before anything else
	configure_logging();

	// grab some needed command line options
	Disk_image_filename = get_cmdline_option(argv, argv + argc, "-d", "--disk");  // will always go to drive one for now

	// initialize SDL before everything else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return -1;
	}

	reset_machine();
	if (Auto_start == true) {
		Emulator_state = emulator_state::EMULATOR_STARTED;
	}

	uint16_t prog_start = 0x600;
	uint16_t load_addr = 0x0;

	const char *prog_start_string = get_cmdline_option(argv, argv + argc, "-p", "--pc");
	if (prog_start_string != nullptr) {
		prog_start = (uint16_t)strtol(prog_start_string, nullptr, 16);
	}

	const char *load_addr_string = get_cmdline_option(argv, argv + argc, "-b", "--base");
	if (load_addr_string != nullptr) {
		load_addr = (uint16_t)strtol(load_addr_string, nullptr, 16);
	}

	const char *binary_file = nullptr;
	binary_file = get_cmdline_option(argv, argv + argc, "-b", "--binary");
	if (binary_file != nullptr) {
		FILE *fp = fopen(binary_file, "rb");
		if (fp == nullptr) {
			printf("Unable to open %s for reading: %d\n", binary_file, errno);
			exit(-1);
		}
		fseek(fp, 0, SEEK_END);
		auto buffer_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		ASSERT(buffer_size <= UINT16_MAX);

		// allocate the memory buffer
		uint8_t *buffer = new uint8_t[buffer_size];
		if (buffer == nullptr) {
			printf("Unable to allocate %ld bytes for source code file buffer\n", buffer_size);
			fclose(fp);
			exit(-1);
		}
		fread(buffer, 1, buffer_size, fp);
		fclose(fp);
		memory_load_buffer(buffer, (uint16_t)buffer_size, load_addr);
		cpu.set_pc(prog_start);
	}

	bool quit = false;
	Total_cycles_this_frame = Total_cycles = 0;
	cpu_sem = SDL_CreateSemaphore(0);
	if (cpu_sem == nullptr) {
		printf("Unable to create semaphore for CPU speed throttling: %s\n", SDL_GetError());
		exit(-1);
	}

	// set up a timer for rendering
	SDL_TimerID render_timer = SDL_AddTimer(16, render_event_timer, nullptr);
	if (render_timer == 0) {
		printf("Unable to create timer for rendering: %s\n", SDL_GetError());
		exit(-1);
	}

	while (!quit) {
		uint32_t cycles_per_frame = CYCLES_PER_FRAME * Speed_multiplier;  // we can speed up machine by multiplier here


		// process the next opcode
		if (Emulator_state == emulator_state::EMULATOR_STARTED) {
			while (true) {
				// process debugger (before opcode processing so that we can break on
				// specific addresses properly
				debugger_process(cpu);

				uint32_t cycles = cpu.process_opcode();
				Total_cycles_this_frame += cycles;
				Total_cycles += cycles;

				if (Total_cycles_this_frame > cycles_per_frame) {
					ui_update_cycle_count();
					// this is essentially number of cycles for one redraw cycle
					// for TV/monitor.  Around 17030 cycles I believe
					Total_cycles_this_frame -= cycles_per_frame;
					SDL_SemWait(cpu_sem);
					break;
				}
			}
		} else {
			debugger_process(cpu);
		}

		{
			SDL_Event evt;

			while (SDL_PollEvent(&evt)) {
				ImGui_ImplSdl_ProcessEvent(&evt);
				switch (evt.type) {
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					keyboard_handle_event(evt);
					break;

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
			video_render_frame();
		}
	}

	SDL_RemoveTimer(render_timer);
	SDL_DestroySemaphore(cpu_sem);

	video_shutdown();
	disk_shutdown();
	keyboard_shutdown();
	joystick_shutdown();
	debugger_shutdown();
	memory_shutdown();

	SDL_Quit();

	return 0;
}

