
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