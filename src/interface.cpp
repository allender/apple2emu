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
#include <cctype>

#include "SDL_image.h"
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
static bool Show_drive_indicators = true;
static bool Menu_open_at_start = false;
static bool Imgui_initialized = false;

static const char *Settings_filename = "settings.txt";

static GLuint Disk_black_texture;
static GLuint Disk_red_texture;

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
			else if (setting == "show_drive_indicators") {
				int i_val = strtol(value.c_str(), nullptr, 10);
				Show_drive_indicators = i_val ? true : false;
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
	fprintf(fp, "show_drive_indicators = %d\n", Show_drive_indicators == true ? 1 : 0);
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
	} else if (old_type != type) {
		// show popup and potentially restart the emulator
		ImGui::OpenPopup("Restart");
	}

	// show the popup if active
	if (ImGui::BeginPopupModal("Restart", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Changing the machine type during\nemulation requires emulator restart\n");
		ImGui::Separator();

		if (ImGui::Button("Restart", ImVec2(120, 0))) {
			// with a restart, set the emulator type,, reset the machine, and for now
			// go to the splash screen
			Emulator_type = static_cast<emulator_type>(type);
			Emulator_state = emulator_state::SPLASH_SCREEN;
			reset_machine();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			type = static_cast<uint8_t>(Emulator_type);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void ui_get_disk_image(uint8_t slot_num)
{
	nfdchar_t *outPath = NULL;
	nfdresult_t result = NFD_OpenDialog("dsk,do,nib", nullptr, &outPath);

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
	ImGui::Checkbox("Show Drive Indicator Lights", &Show_drive_indicators);
	ImGui::Separator();
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
	if (ImGui::Button("Reset Debugger Windows")) {
		debugger_reset_windows();
	}
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

// shows drive status indicators
static void ui_show_drive_indicators()
{
	ImGui::SetNextWindowPos(ImVec2(982.0f, 13.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin("indicators", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoFocusOnAppearing)) {
		uint32_t drive1_track, drive2_track, drive1_sector, drive2_sector;

		GLuint drive1_texture = disk_is_on(1) ? Disk_red_texture : Disk_black_texture;
		GLuint drive2_texture = disk_is_on(2) ? Disk_red_texture : Disk_black_texture;
		disk_get_track_and_sector(1, drive1_track, drive1_sector);
		disk_get_track_and_sector(2, drive2_track, drive2_sector);
		ImGui::Text("%-10s%s", "Drive 1", "Drive 2");
		ImGui::Image((void *)(intptr_t)drive1_texture, ImVec2(16.0f, 16.0f));
		ImGui::SameLine();
		ImGui::Text("%02d/%02d", drive1_track, drive1_sector);
		ImGui::SameLine();
		ImGui::Image((void *)(intptr_t)drive2_texture, ImVec2(16.0f, 16.0f));
		ImGui::SameLine();
		ImGui::Text("%02d/%02d", drive2_track, drive2_sector);
		ImGui::End();
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

		// load up icons.  Need to turn these into opengl textures
		// because we are all opengl all the time.
		SDL_Surface *icon = IMG_Load("interface/black_disk.png");
		if (icon != nullptr) {
			int mode = GL_LOAD_FORMAT_RGB;
			if (icon->format->BytesPerPixel == 4) {
			  mode = GL_LOAD_FORMAT_RGBA;
			}

			glGenTextures(1, &Disk_black_texture);
			glBindTexture(GL_TEXTURE_2D, Disk_black_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon->w, icon->h, 0, mode, GL_UNSIGNED_BYTE, icon->pixels);

			// free the surface since we now have a texture
			SDL_FreeSurface(icon);
		}
		icon = IMG_Load("interface/red_disk.png");
		if (icon != nullptr) {
			int mode = GL_LOAD_FORMAT_RGB;
			if (icon->format->BytesPerPixel == 4) {
			  mode = GL_LOAD_FORMAT_RGBA;
			}

			glGenTextures(1, &Disk_red_texture);
			glBindTexture(GL_TEXTURE_2D, Disk_red_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon->w, icon->h, 0, mode, GL_UNSIGNED_BYTE, icon->pixels);

			// free the surface since we now have a texture
			SDL_FreeSurface(icon);
		}
		Imgui_initialized = true;
	}

	ImGui_ImplSdl_NewFrame(window);

	// handle key presses.  Some keys are global and some
	// will be specific to the window that is current in focus
	ImGuiIO& io = ImGui::GetIO();

	// process high level keys (regardless of which window has focus)
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F11, false)) {
		io.WantCaptureKeyboard = true;
		if (io.KeyShift == true) {
			ui_toggle_demo_window();
		} else {
			debugger_enter();
		}
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F10, false)) {
		if (io.KeyShift == true) {
			Frames_per_second *= 2;
			io.WantCaptureKeyboard = false;

		}
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F9, false)) {
		if (io.KeyShift == true) {
			Frames_per_second /= 2;
			io.WantCaptureKeyboard = false;
		}
	}

	// maybe bring up the main menu
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F1, false)) {
		io.WantCaptureKeyboard = true;
		ui_toggle_main_menu();
	}


	if (ImGui::IsKeyPressed(SDL_SCANCODE_PAUSE, false)) {
		io.WantCaptureKeyboard = true;
		if (Emulator_state == emulator_state::EMULATOR_STARTED) {
			Emulator_state = emulator_state::EMULATOR_PAUSED;
		} else {
			Emulator_state = emulator_state::EMULATOR_STARTED;
		}
	}

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
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;

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

		// display drive indicators if they should be active
		if (Show_drive_indicators == true) {
			ui_show_drive_indicators();
		}
	}
	ImGui::End();
	if (dbg_active == false) {
		ImGui::PopStyleVar(4);
	}

	// send keys to the emulator (if they haven't been used by any of the other
	// imgui windows.  Data needs to be pulled from the input buffer (not not just
	// from the keydown events) because SDL can only disciminate a 1 from a ! (for
	// instance) in the textinput event.  In order to handle various keyboards
	// we'll grab the data from the textinput event (which is stored in
	// the InputCharacters array in the imgui IO structure.  We will need to
	// also use keydown events for ctrl characters, esc, and a few other
	// characters that the emulator will need
	if (io.WantCaptureKeyboard == false) {
		for (auto i = 0; i < 8; i++) {
			uint16_t utf8_char = io.InputCharacters[i];
			if (utf8_char != 0 && utf8_char < 128) {
				keyboard_handle_event(io.InputCharacters[i], false, false, false, false);
			}
		}

		// send keys that are down (which we need for control keys  backspace
		// etc.  The underlying code will figure out what it needs to keep
		for (auto i = 0; i < 512; i++) {
			int key = SDL_GetKeyFromScancode((SDL_Scancode)i);
			if (ImGui::IsKeyPressed(i) && !isprint(key)) {
				keyboard_handle_event(key, io.KeyShift, io.KeyCtrl, io.KeyAlt, io.KeySuper);
			}
		}
	}

	if (Show_demo_window) {
		ImGui::ShowTestWindow();
		ImGui::ShowMetricsWindow();
	}

	ImGui::Render();
}
