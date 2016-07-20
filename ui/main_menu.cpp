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
#include "main_menu.h"
#include "6502/video.h"
#include "6502/disk.h"
#include "utils/path_utils.h"
#include "apple2emu.h"
#include "nfd.h"

bool Show_main_menu = false;
bool Show_disk_menu = false;

static void ui_show_general_options()
{
	static bool auto_start = true;

	ImGui::Text("General Options:");
	ImGui::Checkbox("Auto Start", &auto_start);
	ImGui::SameLine(200);
	if (ImGui::Button("Reboot")) {
		reset_machine();
	}
}

static void ui_get_disk_image(uint8_t slot_num)
{
	nfdchar_t *outPath = NULL;
	nfdresult_t result = NFD_OpenDialog("dsk", nullptr, &outPath);

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
	static int mono_type = 0;
	ImGui::Text("Video Output Options:");
	ImGui::RadioButton("White", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_WHITE));
	ImGui::SameLine();
	ImGui::RadioButton("Amber", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_AMBER));
	ImGui::SameLine();
	ImGui::RadioButton("Green", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_GREEN));
	//ImGui::RadioButton("Color", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_GREEN));
	video_set_mono_type(static_cast<video_mono_types>(mono_type));
}

static void ui_show_main_menu()
{
	ImGui::Begin("Options");
	ui_show_general_options();
	ImGui::Separator();
	ui_show_video_output_menu();
	ImGui::Separator();
	ui_show_disk_menu();
	ImGui::End();

#if 0
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Reboot")) {
				reset_machine();
			}
			//if (ImGui::MenuItem("Open", "Ctrl+O")) {}
			//if (ImGui::MenuItem("Save", "Ctrl+S")) {}
			//if (ImGui::MenuItem("Save As..")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "")) {
				// add quit event to SDL queue
				SDL_Event evt;
				evt.type = SDL_QUIT;
				evt.quit.type = SDL_QUIT;
				SDL_PushEvent(&evt);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options")) {
			if (ImGui::BeginMenu("Monochrome Color")) {
				static int mono_type = 0;
				ImGui::RadioButton("White", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_WHITE));
				ImGui::RadioButton("Amber", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_AMBER));
				ImGui::RadioButton("Green", &mono_type, static_cast<uint8_t>(video_mono_types::MONO_GREEN));
				ImGui::EndMenu();
				video_set_mono_type(static_cast<video_mono_types>(mono_type));
			}
			ImGui::EndMenu();
		}

		// disk menu - for mounting disk images
		if (ImGui::BeginMenu("Disks")) {
			ui_show_disk_menu();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
#endif
}

void ui_init(SDL_Window *window)
{
	ImGui_ImplSdl_Init(window);
}

void ui_shutdown()
{
	ImGui_ImplSdl_Shutdown();
}

void ui_do_frame(SDL_Window *window)
{
	ImGui_ImplSdl_NewFrame(window);
	if (Show_main_menu) {
		ui_show_main_menu();
	}
	if (Show_disk_menu) {
		ui_show_disk_menu();
	}
	//ImGui::ShowTestWindow();
	ImGui::Render();
}

void ui_toggle_main_menu()
{
	Show_main_menu = !Show_main_menu;
}

void ui_toggle_disk_menu()
{
	Show_disk_menu = !Show_disk_menu;
}
