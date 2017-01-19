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

#include <stdio.h>
#include <string>
#include <fstream>

#include "apple2emu_defs.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "interface.h"
#include "video.h"
#include "disk.h"
#include "path_utils.h"
#include "apple2emu.h"
#include "nfd.h"

static bool Show_main_menu = false;
static bool Show_debug_menu = false;
static bool Menu_open_at_start = false;
static bool Imgui_initialized = false;

static const char *Settings_filename = "settings.txt";

// settings to be stored
static int32_t Video_color_type = static_cast<int>(video_display_types::MONO_WHITE);

static const uint32_t Cycles_array_size = 64;
static uint32_t Cycles_per_frame[Cycles_array_size];
static uint32_t Current_cycles_array_entry = 0;

// inserts disk image into the given disk drive
static void ui_insert_disk(const char *disk_filename, int slot)
{
	FILE *fp = fopen(disk_filename, "rb");
	if (fp == nullptr) {
		return;
	}
	fclose(fp);

	disk_insert(disk_filename, slot);
}

static void ui_load_settings()
{
	std::ifstream infile(Settings_filename);
	std::string line;

	while(std::getline(infile, line)) {
		int pos = line.find('=');
		if (pos > 0) {
			std::string setting = line.substr(0, pos - 1);
			std::string value = line.substr(pos + 1);
			while (value[0] == ' ') {
				value.erase(0, 1);
			}

			if (setting == "auto_start") {
				int i_val = strtol(value.c_str(), nullptr, 10);
				Auto_start = i_val ? true : false;
			}
			else if (setting == "emulator_type") {
				int i_val = strtol(value.c_str(), nullptr, 10);
				Emulator_type = static_cast<emulator_type>(i_val);
			}
			else if (setting == "open_at_start") {
				int i_val = strtol(value.c_str(), nullptr, 10);
				Menu_open_at_start = i_val ? true : false;
				Show_main_menu = Menu_open_at_start;
			}
			else if (setting == "disk1") {
				ui_insert_disk(value.c_str(), 1);
			}
			else if (setting == "disk2") {
				ui_insert_disk(value.c_str(), 2);
			}
			else if (setting == "video") {
				Video_color_type = (uint8_t)strtol(value.c_str(), nullptr, 10);
				video_set_mono_type(static_cast<video_display_types>(Video_color_type));
			}
			else if (setting == "speed") {
				Speed_multiplier = (int)strtol(value.c_str(), nullptr, 10);
			}
		}
	}
}

static void ui_save_settings()
{
	FILE *fp = fopen(Settings_filename, "wt");
	if (fp == nullptr) {
		printf("Unable to open settings file for writing\n");
		return;
	}
	fprintf(fp, "auto_start = %d\n", Auto_start == true ? 1 : 0);
	fprintf(fp, "emulator_type = %d\n", static_cast<uint8_t>(Emulator_type));
	fprintf(fp, "open_at_start = %d\n", Menu_open_at_start == true ? 1 : 0);
	fprintf(fp, "disk1 = %s\n", disk_get_mounted_filename(1));
	fprintf(fp, "disk2 = %s\n", disk_get_mounted_filename(2));
	fprintf(fp, "video = %d\n", Video_color_type);
	fprintf(fp, "speed = %d\n", Speed_multiplier);

	fclose(fp);
}

static void ui_show_general_options()
{
	ImGui::Text("General Options:");
	ImGui::Checkbox("Auto Start", &Auto_start);
	ImGui::SameLine(200);
	if (Emulator_state == emulator_state::SPLASH_SCREEN) {
		if (ImGui::Button("Start")) {
			Emulator_state = emulator_state::EMULATOR_STARTED;
		}
	} else {
		if (ImGui::Button("Reboot")) {
			reset_machine();
			Show_main_menu = true;
		}
	}
	ImGui::Checkbox("Open Menu on startup", &Menu_open_at_start);
	ImGui::Separator();

	static int type = static_cast<uint8_t>(Emulator_type);
	int old_type = type;
	ImGui::ListBox("Emulation Type", &type, Emulator_names, static_cast<uint8_t>(emulator_type::NUM_EMULATOR_TYPES));

	// if the emulator type changed, then reset the machine (if we haven't
	// started yet.  Otherwise tell user that we need to reset machine
	// for this change to take effect
	if (old_type != type && Emulator_state == emulator_state::SPLASH_SCREEN) {
		Emulator_type = static_cast<emulator_type>(type);
		reset_machine();
	} else {
	}
}

static void ui_get_disk_image(uint8_t slot_num)
{
	nfdchar_t *outPath = NULL;
	nfdresult_t result = NFD_OpenDialog("dsk,do", nullptr, &outPath);

	if (result == NFD_OKAY) {
		disk_insert(outPath, slot_num);
		free(outPath);
	}
}

static void ui_show_disk_menu()
{
	ImGui::Text("Disk Drive Options:");
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Text("Slot 6, Disk 1:");
	std::string filename;
	path_utils_get_filename(disk_get_mounted_filename(1), filename);
	if (filename.empty()) {
		filename = "<none>";
	}
	ImGui::SameLine();
	if (ImGui::Button(filename.c_str())) {
		ui_get_disk_image(1);
	}
	ImGui::SameLine();
	if (ImGui::Button("Eject")) {
		disk_eject(1);
	}
	ImGui::Spacing();

	ImGui::Text("Slot 6, Disk 2:");
	path_utils_get_filename(disk_get_mounted_filename(2), filename);
	if (filename.empty()) {
		filename = "<none>";
	}
	ImGui::SameLine();
	if (ImGui::Button(filename.c_str())) {
		ui_get_disk_image(2);
	}
	ImGui::SameLine();
	if (ImGui::Button("Eject")) {
		disk_eject(2);
	}
}

static void ui_show_video_output_menu()
{
	ImGui::Text("Video Output Options:");
	ImGui::RadioButton("White", &Video_color_type, static_cast<uint8_t>(video_display_types::MONO_WHITE));
	ImGui::SameLine();
	ImGui::RadioButton("Amber", &Video_color_type, static_cast<uint8_t>(video_display_types::MONO_AMBER));
	ImGui::SameLine();
	ImGui::RadioButton("Green", &Video_color_type, static_cast<uint8_t>(video_display_types::MONO_GREEN));
	ImGui::SameLine();
	ImGui::RadioButton("Color", &Video_color_type, static_cast<uint8_t>(video_display_types::COLOR));
	video_set_mono_type(static_cast<video_display_types>(Video_color_type));
}

static void ui_show_speed_menu()
{
	if (ImGui::SliderInt("Emulator Speed", (int *)&Speed_multiplier, 1, 100) == true) {
	}
}

static void ui_show_main_menu()
{
	ImGui::Begin("Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders);
	ui_show_general_options();
	ImGui::Separator();
	ui_show_video_output_menu();
	ImGui::Separator();
	ui_show_disk_menu();
	ImGui::Separator();
	ui_show_speed_menu();
	ImGui::End();
}

static void ui_show_debug_menu()
{
	static bool animate = true;
	static float cycles[Cycles_array_size];

	ImGui::Begin("Debug");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Separator();
	ImGui::Checkbox("Animate", &animate);
	if (animate == true) {
		auto index = 0;
		auto i = Current_cycles_array_entry;
		while (true) {
			cycles[index++] = static_cast<float>(Cycles_per_frame[i]);
			i = (i + 1) % Cycles_array_size;
			if (i == Current_cycles_array_entry) {
				break;
			}
		}
	}
	ImGui::PlotLines("Cycles/Frame", cycles, Cycles_array_size, 0, nullptr, 17020.0f, 17050.0f, ImVec2(0, 50));
	ImGui::End();
}

void ui_init()
{
	static bool settings_loaded = false;

	// only load settings when we first start
	// the emulator
	if (settings_loaded == false) {
		ui_load_settings();
		settings_loaded = true;
	}
	for (auto i = 0; i < Cycles_array_size; i++) {
		Cycles_per_frame[i] = 0;
	}
}

void ui_shutdown()
{
	ImGui_ImplSdl_Shutdown();
	Imgui_initialized = false;
	ui_save_settings();
}

void ui_do_frame(SDL_Window *window)
{
	// initlialize imgui if it has not been initialized yet
	if (Imgui_initialized == false) {
		ImGui_ImplSdl_Init(window);

		// maybe use apple 2 font here
		//ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->AddFontFromFileTTF("printchar21.ttf", 8.0f);
		Imgui_initialized = true;
	}

	ImGui_ImplSdl_NewFrame(window);
	if (Show_main_menu) {
		ui_show_main_menu();
	}
	if (Show_debug_menu) {
		ui_show_debug_menu();
	}
	//ImGui::ShowTestWindow();

	if (Show_main_menu || Show_debug_menu) {
		ImGui::Render();
	}
}

void ui_toggle_main_menu()
{
	Show_main_menu = !Show_main_menu;
}

void ui_toggle_debug_menu()
{
	Show_debug_menu = !Show_debug_menu;
}

// since rendering is decoupled from cycles on the machine, extra
// function to store off cycles for debug display
void ui_update_cycle_count()
{
	Cycles_per_frame[Current_cycles_array_entry] = Total_cycles_this_frame;
	Current_cycles_array_entry = (Current_cycles_array_entry + 1) % Cycles_array_size;
}

