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

#pragma once

#include "SDL.h"
#include "SDL_opengl.h"

// video modes
#define VIDEO_MODE_TEXT    (1 << 0)
#define VIDEO_MODE_MIXED   (1 << 1)
#define VIDEO_MODE_PAGE2   (1 << 2)
#define VIDEO_MODE_HIRES   (1 << 3)
#define VIDEO_MODE_80COL   (1 << 4)
#define VIDEO_MODE_ALTCHAR (1 << 5)

// display types (mono)
enum class video_display_types : uint8_t {
	MONO_WHITE = 0,
	MONO_AMBER,
	MONO_GREEN,
	NUM_MONO_TYPES,
	COLOR,
};


extern SDL_Renderer *Video_renderer;
extern uint8_t Video_mode;
extern GLuint Video_framebuffer_texture;
extern SDL_Rect Video_window_size;

bool video_init();
void video_shutdown();
void video_render_frame();
void video_set_mono_type(video_display_types type);


// called from soft switch reading/writing code in memory
uint8_t video_set_state(uint16_t addr, uint8_t val, bool write);
uint8_t video_get_state(uint16_t addr, uint8_t val, bool write);
