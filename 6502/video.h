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

// temporary header file for basic text I/O using pdcurses for windows

#pragma once

#include "SDL.h"
#include "memory.h"

#if !defined(USE_SDL)

void init_text_screen();
void set_screen_size(int *lines, int *columns);
void clear_screen();
void move_cursor(int row, int col);
void set_raw(bool state);
void output_char(char c);
void screen_bottom();
void scroll_screen();
void set_inverse();
void set_flashing();
void set_normal();

#else

extern SDL_Window *Video_window;
extern SDL_Renderer *Video_renderer;

bool video_init(memory &mem);
void video_shutdown();
void video_render_frame(memory &mem);

#endif // USE_SDL