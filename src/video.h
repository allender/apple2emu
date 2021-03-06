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

// there is something weird with SDL/SDL_image
// on mac (maybe linux) where textures are coming
// back as BGR format instead of RGB.  Maybe it's
// how SDL_image was compiled.  Too lazy to look
// more info it.  Define modes for RGB and RGBA
// that can be used when loading with SDL_image
#if defined(__APPLE__)
#define GL_LOAD_FORMAT_RGB GL_BGR
#define GL_LOAD_FORMAT_RGBA GL_BGRA
#else
#define GL_LOAD_FORMAT_RGB GL_RGB
#define GL_LOAD_FORMAT_RGBA GL_RGBA
#endif

// display types (mono)
enum class video_tint_types : uint8_t {
	MONO_WHITE = 0,
	MONO_AMBER,
	MONO_GREEN,
	COLOR,
	TINT_TYPE_NONE,
	NUM_TINT_TYPES,
};

extern SDL_Renderer *Video_renderer;
extern uint8_t Video_mode;
extern SDL_Rect Video_window_size;
extern const int Video_native_width;
extern const int Video_native_height;

bool video_init();
void video_shutdown();
void video_render_frame();
void video_set_tint(video_tint_types type);
GLfloat *video_get_tint(video_tint_types type = video_tint_types::TINT_TYPE_NONE);

// called from soft switch reading/writing code in memory
uint8_t video_set_state(uint16_t addr, uint8_t val, bool write);
uint8_t video_get_state(uint16_t addr, uint8_t val, bool write);
