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

bool Show_main_menu = false;

static void ui_show_main_menu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
			 ImGui::MenuItem("(dummy menu)", NULL, false, false);
			 if (ImGui::MenuItem("New")) {}
			 if (ImGui::MenuItem("Open", "Ctrl+O")) {}
			 if (ImGui::BeginMenu("Open Recent"))
			 {
				  ImGui::MenuItem("fish_hat.c");
				  ImGui::MenuItem("fish_hat.inl");
				  ImGui::MenuItem("fish_hat.h");
				  if (ImGui::BeginMenu("More.."))
				  {
						ImGui::MenuItem("Hello");
						ImGui::MenuItem("Sailor");
				  }
				  ImGui::EndMenu();
			 }
			 if (ImGui::MenuItem("Save", "Ctrl+S")) {}
			 if (ImGui::MenuItem("Save As..")) {}
			 ImGui::Separator();
			 if (ImGui::BeginMenu("Options"))
			 {
				  static bool enabled = true;
				  ImGui::MenuItem("Enabled", "", &enabled);
				  ImGui::BeginChild("child", ImVec2(0, 60), true);
				  for (int i = 0; i < 10; i++)
						ImGui::Text("Scrolling Text %d", i);
				  ImGui::EndChild();
				  static float f = 0.5f;
				  static int n = 0;
				  ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
				  ImGui::InputFloat("Input", &f, 0.1f);
				  ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
				  ImGui::EndMenu();
			 }
			 if (ImGui::BeginMenu("Colors"))
			 {
				  for (int i = 0; i < ImGuiCol_COUNT; i++)
						ImGui::MenuItem(ImGui::GetStyleColName((ImGuiCol)i));
				  ImGui::EndMenu();
			 }
			 if (ImGui::BeginMenu("Disabled", false)) // Disabled
			 {
				  IM_ASSERT(0);
			 }
			 if (ImGui::MenuItem("Checked", NULL, true)) {}
			 if (ImGui::MenuItem("Quit", "Alt+F4")) {}
						ImGui::EndMenu();
		  }
        //if (ImGui::BeginMenu("Edit"))
        //{
        //    if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
        //    if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
        //    ImGui::Separator();
        //    if (ImGui::MenuItem("Cut", "CTRL+X")) {}
        //    if (ImGui::MenuItem("Copy", "CTRL+C")) {}
        //    if (ImGui::MenuItem("Paste", "CTRL+V")) {}
        //    ImGui::EndMenu();
        //}
        ImGui::EndMainMenuBar();
    }
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
	//ImGui::ShowTestWindow();
	ImGui::Render();
}

void ui_toggle_main_menu()
{
	Show_main_menu = !Show_main_menu;
}