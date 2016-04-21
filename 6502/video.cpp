//
// Kind of a temporary file to deal with basic text I/O for now.  Will
// use PDCurses for development
//

#include "6502/curses.h"
#include "panel.h"
#include "video.h"

chtype flag = 0;

PANEL *Main_panel;
bool is_raw = false;

void init_text_screen()
{
	initscr();
	Main_panel = new_panel(stdscr);
	show_panel(Main_panel);
	top_panel(Main_panel);
}

void clear_screen()
{
	clear();
}

void move_cursor(int row, int col)
{
	if (is_raw == true) {
		move(row, col);
	}
}

void set_raw(bool state)
{
	if (state == true) {
		raw();
		noecho();
		scrollok(stdscr, 1);
		timeout(0);
		is_raw = true;
	} else {
		noraw();
		echo();
		timeout(-1);
		is_raw = false;
	}
}

void output_char(char c)
{
	echochar(flag | c);
}


void screen_bottom()
{
	move_cursor(MAX_LINES,0);
}

void scroll_screen()
{
	scrl(1);
}

void set_normal()
{
	flag = A_NORMAL;
}

void set_inverse()
{
	flag = A_REVERSE;
}

void set_flashing()
{
	flag = A_BLINK;
}
