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

#define GLEW_STATIC

#include <GL/glew.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <cctype>
#include <filesystem>

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_image.h"

#include "apple2emu_defs.h"
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl.h"
#include "interface.h"
#include "video.h"
#include "disk.h"
#include "path_utils.h"
#include "apple2emu.h"
#include "nfd.h"
#include "debugger.h"
#include "keyboard.h"
#include "speaker.h"

static bool Show_main_menu = true;
static bool Show_demo_window = false;
static bool Show_drive_indicators = true;
static bool Menu_open_at_start = false;

static int Sound_volume = 50;

static const char *Settings_filename = "settings.txt";

static SDL_Window *Video_window = nullptr;
static SDL_GLContext Video_context;
static GLuint Video_framebuffer;
static GLuint Video_framebuffer_texture;
static SDL_Rect Video_native_size;

static GLuint Disk_black_texture;
static GLuint Disk_red_texture;
static GLuint Splash_screen_texture;

SDL_Rect Video_window_size;
static int Video_scale_factor = 2;

// settings to be stored
static int32_t Video_color_type = static_cast<int>(video_tint_types::MONO_WHITE);

//  information for symbol tables
typedef std::pair<std::string, bool> symtable_entry;
std::vector<symtable_entry> Symtables;

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
	// find the symbol tables stored in the symtble
	for (auto &f : std::filesystem::directory_iterator("symtables")) {
		if (f.is_regular_file() == true) {
			Symtables.push_back(std::make_pair(f.path().relative_path().string(), false));
		}
	}
	// folder
	std::ifstream infile(Settings_filename);
	std::string line;

	while(std::getline(infile, line)) {
		size_t pos = line.find('=');
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
			else if (setting == "sound_volume") {
				int i_val = strtol(value.c_str(), nullptr, 10);
				Sound_volume = i_val;
				speaker_set_volume(Sound_volume);
			}
			else if (setting.rfind("Symtable",0) == 0) {
				// need to get the table name from the setting
				size_t table_pos = line.find(' ');
				std::string table_name = setting.substr(table_pos + 1);
				int i_val = strtol(value.c_str(), nullptr, 10);
				for (auto &p : Symtables) {
					if (p.first == table_name) {
						p.second = static_cast<bool>(i_val);
						if (p.second == true) {
							debugger_load_symbol_table(p.first);
						}
						break;
					}
				}
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
	fprintf(fp, "sound_volume = %d\n", Sound_volume);
	for (auto &table : Symtables) {
		fprintf(fp, "Symtable %s = %d\n", table.first.c_str(), table.second?1:0);
	}

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

	// symbol table submenu to add, remove
    if (ImGui::BeginMenu("Symbol Tables")) {
		for (auto &table : Symtables) {
			//ImGui::MenuItem(table.first.c_str(), "", &table.second);
			bool changed = ImGui::Checkbox(table.first.c_str(), &table.second);

			// load or unload the symbol table
			if (changed == true && table.second == true) {
				debugger_load_symbol_table(table.first);
			} else if (changed == true && table.second == false) {
				debugger_unload_symbol_table(table.first);
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::Button("Reset Debugger Windows")) {
		debugger_reset_windows();
	}
}

static void ui_show_sound_menu()
{
	if (ImGui::SliderInt("Volume", &Sound_volume, 0, 100)) {
		speaker_set_volume(Sound_volume);
	}
}

static void ui_show_speed_menu()
{
	if (ImGui::SliderInt("Emulator Speed", (int *)&Speed_multiplier, 1, 100) == true) {
	}
}

static void ui_show_edit_menu()
{
    if (ImGui::MenuItem("Paste"))
    {
        keyboard_paste_clipboard();
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
        if (ImGui::BeginMenu("Edit")) {
			ui_show_edit_menu();
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
			ui_show_sound_menu();
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
	ImGui::SetNextWindowPos(ImVec2(982.0f, 13.0f), ImGuiCond_FirstUseEver);
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
	IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);

	static bool settings_loaded = false;

	// only load settings when we first start
	// the emulator
	if (settings_loaded == false) {
		ui_load_settings();
		settings_loaded = true;
	}

	Video_native_size.x = 0;
	Video_native_size.y = 0;
	Video_native_size.w = Video_native_width;
	Video_native_size.h = Video_native_height;

	Video_window_size.x = 0;
	Video_window_size.y = 0;
	Video_window_size.w = (int)(Video_scale_factor * Video_native_size.w);
	Video_window_size.h = (int)(Video_scale_factor * Video_native_size.h);


	// set attributes for GL
	uint32_t sdl_window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	Video_window = SDL_CreateWindow("Apple2Emu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Video_window_size.w, Video_window_size.h, sdl_window_flags);
	if (Video_window == nullptr) {
		printf("Unable to create SDL window: %s\n", SDL_GetError());
		return;
	}

	// create openGL context
	if (Video_context != nullptr) {
		SDL_GL_DeleteContext(Video_context);
	}
	Video_context = SDL_GL_CreateContext(Video_window);
	SDL_GL_MakeCurrent(Video_window, Video_context);
	SDL_GL_SetSwapInterval(1);

	if (Video_context == nullptr) {
		printf("Unable to create GL context: %s\n", SDL_GetError());
		return;
	}
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		printf("Unable to initialize glew: %s\n", glewGetErrorString(err));
	}

	// framebuffer and render to texture
	glGenFramebuffers(1, &Video_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, Video_framebuffer);

	glGenTextures(1, &Video_framebuffer_texture);
	glBindTexture(GL_TEXTURE_2D, Video_framebuffer_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Video_native_width, Video_native_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// attach texture to framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Video_framebuffer_texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Unable to create framebuffer for render to texture.\n");
		return;
	}

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(Video_window, Video_context);
    ImGui_ImplOpenGL2_Init();

	// create the splash screen
	SDL_Surface *surface = IMG_Load("interface/splash.jpg");
	if (surface != nullptr) {
		int mode = GL_LOAD_FORMAT_RGB;
		if (surface->format->BytesPerPixel == 4) {
		  mode = GL_LOAD_FORMAT_RGBA;
		}

		glGenTextures(1, &Splash_screen_texture);
		glBindTexture(GL_TEXTURE_2D, Splash_screen_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);
		SDL_FreeSurface(surface);
	}

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
}

void ui_shutdown()
{
	// shut down IMGUI
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

	// and then SDL
    SDL_GL_DeleteContext(Video_context);
    SDL_DestroyWindow(Video_window);

	ui_save_settings();
}

void ui_toggle_main_menu()
{
	Show_main_menu = !Show_main_menu;
}

void ui_toggle_demo_window() {
	Show_demo_window = !Show_demo_window;
}

void ui_do_frame()
{
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(Video_window);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Video_native_width, Video_native_height);
	glClearColor(0.5f, 0.5f, 0.5f, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	// render to the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Video_framebuffer);
	glViewport(0, 0, Video_native_width, Video_native_height);
	glClearColor(0.5f, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, (float)Video_native_width, 0.0f, (float)Video_native_height, 0.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);

	GLfloat *tint_colors;
	if (Emulator_state == emulator_state::SPLASH_SCREEN) {
		// blit splash screen
		glBindTexture(GL_TEXTURE_2D, Splash_screen_texture);
		glColor3f(1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(0, Video_native_height);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(Video_native_width, Video_native_height);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(Video_native_width, 0);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(0, 0);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		tint_colors = video_get_tint(video_tint_types::MONO_WHITE);
	} else {
		extern void video_render();
		video_render();
		tint_colors = video_get_tint();
	}
	ImVec4 tint(tint_colors[0], tint_colors[1], tint_colors[2], 0xff);

	// back to main framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Video_window_size.w, Video_window_size.h);
	glOrtho(0.0f, (float)Video_window_size.w, (float)Video_window_size.h, 0.0f, 0.0f, 1.0f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	ImGui::NewFrame();

	// handle key presses.  Some keys are global and some
	// will be specific to the window that is current in focus
	ImGuiIO& io = ImGui::GetIO();

	// show the main window for the emulator.  If the debugger
	// is active, we'll wind up showing the emulator screen
	// in imgui window that can be moved/resized.  Otherwise
	// we'll show it fullscreen
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
		flags = ImGuiWindowFlags_NoScrollbar;
		ImGui::SetNextWindowPos(ImVec2(2, 2), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(initial_size, ImGuiCond_FirstUseEver);
	} else {
		title = "Emulator";
		initial_size.x = (float)Video_window_size.w;
		initial_size.y = (float)Video_window_size.h;
		flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;

		// also need to set position here
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(initial_size, ImGuiCond_Always);
	}

	// with no debugger, turn off all padding in the window
	if (dbg_active == false) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	}

	//if (ImGui::Begin(title, nullptr, initial_size, -1.0f, flags)) {
	if (ImGui::Begin(title, nullptr, flags)) {
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

	// process high level keys (regardless of which window has focus)
	if (dbg_active == false && ImGui::IsKeyPressed(SDL_SCANCODE_F11, false)) {
		io.WantCaptureKeyboard = true;
		if (io.KeyShift == true) {
			ui_toggle_demo_window();
		} else {
			debugger_enter();
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

	// send keys to the emulator (if they haven't been used by any of the other
	// imgui windows.  Data needs to be pulled from the input buffer (not not just
	// from the keydown events) because SDL can only disciminate a 1 from a ! (for
	// instance) in the textinput event.  In order to handle various keyboards
	// we'll grab the data from the textinput event (which is stored in
	// the InputCharacters array in the imgui IO structure.  We will need to
	// also use keydown events for ctrl characters, esc, and a few other
	// characters that the emulator will need
	if (io.WantCaptureKeyboard == false) {
        // get the text input keys from IMGUI and send whatever has been typed as normal
        // keys.  special handling of ctrl keys, etc is done next
        if (io.KeyCtrl == false && io.KeyAlt == false && io.KeySuper == false) {
            for (auto n = 0; n < io.InputQueueCharacters.Size; n++)
            {
                unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                keyboard_handle_event(c, false, io.KeyCtrl, io.KeyAlt, io.KeySuper);
            }
        }

        // now handle non-printable characters
		for (auto i = 0; i < 512; i++) {
			int key = SDL_GetKeyFromScancode((SDL_Scancode)i);
            if (io.KeyCtrl == true  || (key <= SDLK_a || key > SDLK_z)) {
                if (ImGui::IsKeyPressed(i)) {
                    keyboard_handle_event(key, io.KeyShift, io.KeyCtrl, io.KeyAlt, io.KeySuper);
                }
            }
		}

	}

	if (Show_main_menu) {
		ui_show_main_menu();
	}

	if (Show_demo_window) {
		ImGui::ShowDemoWindow();
		ImGui::ShowMetricsWindow();
	}

	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(Video_window);
}
