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
#include "debugger.h"
#include "keyboard.h"

static bool Show_main_menu = false;
static bool Show_demo_window = false;
static bool Menu_open_at_start = false;
static bool Imgui_initialized = false;

static const char *Settings_filename = "settings.txt";

// settings to be stored
static int32_t Video_color_type = static_cast<int>(video_tint_types::MONO_WHITE);

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
				video_set_tint(static_cast<video_tint_types>(Video_color_type));
			}
			else if (setting == "speed") {
				Speed_multiplier = (int)strtol(value.c_str(), nullptr, 10);
			}
			else if (setting == "sym_tables") {
				Debugger_use_sym_tables = strtol(value.c_str(), nullptr, 10) ? true : false;
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
	fprintf(fp, "sym_tables = %d\n", Debugger_use_sym_tables);

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
	if (old_type != type && (Emulator_state == emulator_state::SPLASH_SCREEN ||
		Emulator_state == emulator_state::EMULATOR_TEST)) {
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
	// total hack.  double mouse clicks (at least on windows)
	// is causing menu to lose focus when dialog goes away
	// because SDL is picking up mouse clikc from dialog and
	// that mouse click brings focus to the emulator window.
	// This code eats that mouse click for this case to keep
	// focus on the menu
	static bool new_image = false;
	if (new_image == true) {
		ImGuiIO& io = ImGui::GetIO();
		if (io.MouseClicked[0]) {
			io.MouseClicked[0] = false;
		}
		new_image = false;
	}
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
		new_image = true;
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
		new_image = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Eject")) {
		disk_eject(2);
	}
}

static void ui_show_video_output_menu()
{
	ImGui::Text("Video Output Options:");
	ImGui::RadioButton("White", &Video_color_type, static_cast<uint8_t>(video_tint_types::MONO_WHITE));
	ImGui::RadioButton("Amber", &Video_color_type, static_cast<uint8_t>(video_tint_types::MONO_AMBER));
	ImGui::RadioButton("Green", &Video_color_type, static_cast<uint8_t>(video_tint_types::MONO_GREEN));
	ImGui::RadioButton("Color", &Video_color_type, static_cast<uint8_t>(video_tint_types::COLOR));
	video_set_tint(static_cast<video_tint_types>(Video_color_type));
}

static void ui_show_debugger_menu()
{
	ImGui::Text("Debugger Options");
	ImGui::Checkbox("Use Symbol Tables", &Debugger_use_sym_tables);
}

static void ui_show_speed_menu()
{
	if (ImGui::SliderInt("Emulator Speed", (int *)&Speed_multiplier, 1, 100) == true) {
	}
}

static void ui_show_main_menu()
{
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ui_show_general_options();
			ui_show_speed_menu();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Disk")) {
			ui_show_disk_menu();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Video")) {
			ui_show_video_output_menu();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Sound")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debugger")) {
			ui_show_debugger_menu();
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
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
}

void ui_shutdown()
{
	ImGui_ImplSdl_Shutdown();
	Imgui_initialized = false;
	ui_save_settings();
}

void ui_toggle_main_menu()
{
	Show_main_menu = !Show_main_menu;
}

void ui_toggle_demo_window() {
	Show_demo_window = !Show_demo_window;
}

void ui_do_frame(SDL_Window *window)
{
	// initlialize imgui if it has not been initialized yet
	if (Imgui_initialized == false) {
		ImGui_ImplSdl_Init(window);
		Imgui_initialized = true;
	}

	ImGui_ImplSdl_NewFrame(window);

	if (Show_main_menu) {
		ui_show_main_menu();
	}

	// show the main window for the emulator.  If the debugger
	// is active, we'll wind up showing the emulator screen
	// in imgui window that can be moved/resized.  Otherwise
	// we'll show it fullscreen
	GLfloat *tint_colors = video_get_tint();
	ImVec4 tint(tint_colors[0], tint_colors[1], tint_colors[2], 0xff);
	ImVec2 initial_size;
	const char *title;
	int flags = 0;

	bool dbg_active = debugger_active();

	if (dbg_active) {
		debugger_render();

		// set flags for rendering main window
		title = "Emulator Debugger";
		initial_size.x = (float)(Video_window_size.w/2);
		initial_size.y = (float)(Video_window_size.h/2);
		flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_ShowBorders;
		ImGui::SetNextWindowPos(ImVec2(5.0f, 6.0f), ImGuiSetCond_FirstUseEver);
	} else {
		title = "Emulator";
		initial_size.x = (float)Video_window_size.w;
		initial_size.y = (float)Video_window_size.h;
		flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize;

		// also need to set position here
		ImVec2 pos(0.0f, 0.0f);
		ImGui::SetNextWindowPos(pos, ImGuiSetCond_Always);
	}

	// with no debugger, turn off all padding in the window
	if (dbg_active == false) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	}

	if (ImGui::Begin(title, nullptr, initial_size, -1.0f, flags)) {
		// use the content region for the window which will scale
		// the texture apporpriately
		ImVec2 content_region = ImGui::GetContentRegionAvail();
		ImGui::Image((void *)(intptr_t)Video_framebuffer_texture,
				content_region,
				ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
				tint);
	}
	ImGui::End();
	if (dbg_active == false) {
		ImGui::PopStyleVar(4);
	}

	// see if we should be sending keys to the main window.  If
	// want capture keyboard is false, then we should send the
	// key information to the keyboard code since it wasn't
	// used elsewhere in imgui
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard == false) {
		for (auto i = 0; i < 512; i++) {
			if (io.KeysDown[i] && io.KeysDownDuration[i] == 0.0f) {
				// key was just pressed, to add to keyboard buffer
				keyboard_handle_event(i, io.KeyShift, io.KeyCtrl, io.KeyAlt, io.KeySuper);
			}
		}
	}

	if (Show_demo_window) {
		ImGui::ShowTestWindow();
		ImGui::ShowMetricsWindow();
	}

	ImGui::Render();
}
