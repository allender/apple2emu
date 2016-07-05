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

#pragma once

#include "SDL.h"

#define MAX_TEXT_LINES   24
#define MAX_TEXT_COLUMNS 40

// video modes
#define VIDEO_MODE_TEXT    (1 << 0)
#define VIDEO_MODE_MIXED   (1 << 1)
#define VIDEO_MODE_PRIMARY (1 << 2)
#define VIDEO_MODE_HIRES   (1 << 3)

// display types (mono)
enum class video_mono_types : uint8_t {
   MONO_WHITE = 0,
   MONO_AMBER,
   MONO_GREEN,
   NUM_MONO_TYPES,
};


extern SDL_Window *Video_window;
extern SDL_Renderer *Video_renderer;
extern uint8_t Video_mode;

bool video_init();
void video_shutdown();
void video_render_frame();
void video_resize(bool scale_up = true);
void video_set_mono_type(video_mono_types type);


