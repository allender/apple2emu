
// temporary header file for basic text I/O using pdcurses for windows

#pragma once

#define MAX_LINES   24
#define MAX_COLUMNS 40

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
