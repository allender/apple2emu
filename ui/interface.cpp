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

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "interface.h"
#include "6502/video.h"
#include "6502/disk.h"
#include "utils/path_utils.h"
#include "apple2emu.h"
#include "nfd.h"

bool Show_main_menu = false;
bool Show_debug_menu = false;

static const int32_t Line_length = 256;
static const char *Settings_filename = "settings.txt";

// settings to be stored
static bool Auto_start = false;
static int32_t Video_color_type = static_cast<int>(video_display_types::MONO_WHITE);
static int32_t Emulator_speed = 1;

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
	char line[Line_length];
	FILE *fp = fopen(Settings_filename, "rt");
	if (fp == nullptr) {
		return;
	}
	while (!feof(fp)) {
		char *ptr = fgets(line, Line_length, fp);
		if (ptr != nullptr) {
			char setting[Line_length], value[Line_length];
			int num_scanned = sscanf(line, "%s = %s\n", setting, value);
			if (num_scanned == 2) {
				if (!stricmp(setting, "auto_start")) {
					int i_val = strtol(value, nullptr, 10);
					Auto_start = i_val ? true : false;
				}
				else if (!stricmp(setting, "disk1")) {
					ui_insert_disk(value, 1);
				}
				else if (!stricmp(setting, "disk2")) {
					ui_insert_disk(value, 2);
				}
				else if (!stricmp(setting, "video")) {
					Video_color_type = (uint8_t)strtol(value, nullptr, 10);
					video_set_mono_type(static_cast<video_display_types>(Video_color_type));
				}
				else if (!stricmp(setting, "speed")) {
					Emulator_speed = (int)strtol(value, nullptr, 10);
					set_emulator_speed(Emulator_speed);
				}
			}

		}
	}

	fclose(fp);
}

static void ui_save_settings()
{
	FILE *fp = fopen(Settings_filename, "wt");
	if (fp == nullptr) {
		printf("Unable to open settings file for writing\n");
		return;
	}
	fprintf(fp, "auto_start = %d\n", Auto_start == true ? 1 : 0);
	fprintf(fp, "disk1 = %s\n", disk_get_mounted_filename(1));
	fprintf(fp, "disk2 = %s\n", disk_get_mounted_filename(2));
	fprintf(fp, "video = %d\n", Video_color_type);
	fprintf(fp, "speed = %d\n", Emulator_speed);

	fclose(fp);
}


static void ui_show_general_options()
{
	ImGui::Text("General Options:");
	ImGui::Checkbox("Auto Start", &Auto_start);
	ImGui::SameLine(200);
	if (ImGui::Button("Reboot")) {
		reset_machine();
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
	if (ImGui::SliderInt("Emulator Speed", &Emulator_speed, 1, 10) == true) {
		set_emulator_speed(Emulator_speed);
	}
}

static void ui_show_main_menu()
{
	ImGui::Begin("Options");
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

void ui_init(SDL_Window *window)
{
	ImGui_ImplSdl_Init(window);
	ui_load_settings();

	for (auto i = 0; i < Cycles_array_size; i++) {
		Cycles_per_frame[i] = 0;
	}
}

void ui_shutdown()
{
	ImGui_ImplSdl_Shutdown();
	ui_save_settings();
}

void ui_do_frame(SDL_Window *window)
{
	ImGui_ImplSdl_NewFrame(window);
	if (Show_main_menu) {
		ui_show_main_menu();
	}
	if (Show_debug_menu) {
		ui_show_debug_menu();
	}
	//ImGui::ShowTestWindow();
	ImGui::Render();
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
