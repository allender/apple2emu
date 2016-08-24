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

#include "6502/cpu.h"
#include "6502/assemble.h"
#include "6502/video.h"
#include "6502/disk.h"
#include "6502/keyboard.h"
#include "6502/joystick.h"
#include "6502/speaker.h"
#include "debugger/debugger.h"
#include "utils/path_utils.h"
#include "ui/interface.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "apple2emu.h"

#include "SDL.h"

#if defined(_MSC_VER)
#pragma warning(disable:4996)   // disable the deprecated warnings for fopen
#endif

INITIALIZE_EASYLOGGINGPP

static const double Render_time = 33;   // roughly 30 fps

const char *Disk_image_filename = nullptr;

uint32_t Total_cycles, Total_cycles_this_frame;
	
cpu_6502 cpu;

static void configure_logging()
{
	el::Configurations conf;

	conf.setToDefault();
	conf.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
	conf.set(el::Level::Global, el::ConfigurationType::LogFileTruncate, "true");
	conf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
	conf.set(el::Level::Global, el::ConfigurationType::Format, "%msg");
	el::Loggers::reconfigureLogger("default", conf);

	// set some options on the logger
	el::Loggers::addFlag(el::LoggingFlag::ImmediateFlush);
	el::Loggers::addFlag(el::LoggingFlag::NewLineForContainer);
	el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
}


static char *get_cmdline_option(char **start, char **end, const std::string &short_option, const std::string &long_option = "")
{
	char **iter = std::find(start, end, short_option);
	if (iter == end) {
		iter = std::find(start, end, long_option);
	}
	if ((iter != end) && (iter++ != end) ) {
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

bool dissemble_6502(const char *filename)
{
	return true;
}

void reset_machine()
{
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
	Disk_image_filename = get_cmdline_option(argv, argv+argc, "-d", "--disk");  // will always go to drive one for now

	// initialize SDL before everything else
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return -1;
	}

	// reset the machine
	reset_machine();

	uint16_t prog_start = 0x600;
	uint16_t load_addr = 0x0;

	const char *prog_start_string = get_cmdline_option(argv, argv+argc, "-p", "--pc");
	if (prog_start_string != nullptr) {
		prog_start = (uint16_t)strtol(prog_start_string, nullptr, 16);
	}

	const char *load_addr_string = get_cmdline_option(argv, argv+argc, "-b", "--base");
	if (load_addr_string != nullptr) {
		load_addr = (uint16_t)strtol(load_addr_string, nullptr, 16);
	}

	const char *binary_file = nullptr;
	binary_file = get_cmdline_option(argv, argv+argc, "-b", "--binary");
	if (binary_file != nullptr) {
		FILE *fp = fopen(binary_file, "rb");
		if (fp == nullptr) {
			printf("Unable to open %s for reading: %d\n", binary_file, errno);
			exit(-1);
		}
		fseek(fp, 0, SEEK_END);
		uint32_t buffer_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// allocate the memory buffer
		uint8_t *buffer = new uint8_t[buffer_size];
		if (buffer == nullptr) {
			printf("Unable to allocate %d bytes for source code file buffer\n", buffer_size);
			fclose(fp);
			exit(-1);
		}
		fread(buffer, 1, buffer_size, fp);
		fclose(fp);
		memory_load_buffer(buffer, buffer_size, load_addr);
		cpu.set_pc(prog_start);
	} else {
	}

	bool quit = false;
	double last_time = SDL_GetTicks();
	double processed_time = 0;
	Total_cycles_this_frame = Total_cycles = 0;

	while (!quit) {

		// process debugger (before opcode processing so that we can break on
		// specific addresses properly
		debugger_process(cpu);

		// process the next opcode
		uint32_t cycles = cpu.process_opcode();
		Total_cycles_this_frame += cycles;
		Total_cycles += cycles;

		if (Total_cycles_this_frame > CYCLES_PER_FRAME) {
			ui_update_cycle_count();
			// this is essentially number of cycles for one redraw cycle
			// for TV/monitor.  Around 17030 cycles I believe
			Total_cycles_this_frame -= CYCLES_PER_FRAME;
		}

		// determine whether to render or not.  Calculate diff
		// between last frame.
		double cur_time = SDL_GetTicks();
		double diff_time = cur_time - last_time;
		last_time = cur_time;

		bool should_render = false;
		processed_time += diff_time;
		while (processed_time > Render_time) {
			should_render = true;
			processed_time -= Render_time;
		}

		if (should_render == true) {
         SDL_Event evt;

			if (SDL_PollEvent(&evt)) {
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

	video_shutdown();
	disk_shutdown();
	keyboard_shutdown();
	joystick_shutdown();
   debugger_shutdown();
	memory_shutdown();

	SDL_Quit();

	return 0;
}

