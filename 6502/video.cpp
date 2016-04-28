//
// Kind of a temporary file to deal with basic text I/O for now.  Will
// use PDCurses for development
//

#if defined(USE_SDL)

#include "SDL.h"
#include "SDL_image.h"
#include "6502/font.h"

//#define SCREEN_W   560
//#define SCREEN_H   384
#define SCREEN_W  1024 
#define SCREEN_H  768 

SDL_Window *Video_window;
SDL_Renderer *Video_renderer;
SDL_TimerID Video_flash_timer;
bool Video_flash = false;
font Video_font, Video_inverse_font;

static char character_conv[] = {

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $00	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $08	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $10	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $18	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $20	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $28	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $30	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $38	*/

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $40	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $48	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $50	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $58	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $60	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $68	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $70	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $78	*/

	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g',		/* $80	*/
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',		/* $88	*/
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',		/* $90	*/
	'x', 'y', 'z', '[', '\\',']', '^', '_',		/* $98	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $A0	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $A8	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $B0	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $B8	*/

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $C0	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $C8	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $D0	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $D8	*/
	' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g',		/* $E0	*/
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',		/* $E8	*/
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',		/* $F0	*/
	'x', 'y', 'z', ';', '<', '=', '>', '?',		/* $F8	*/
};

// callback for flashing characters
static uint32_t timer_flash_callback(uint32_t interval, void *param)
{
	Video_flash = !Video_flash;
	return interval;
}

// intialize the SDL system
bool video_init()
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return false;
	}
		
	// create SDL window
	Video_window = SDL_CreateWindow("Apple2Emu", 100, 100, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
	if (Video_window == nullptr) {
		printf("Unable to create SDL window: %s\n", SDL_GetError());
		SDL_Quit();
		return false;
	}

	// create SDL renderer
	Video_renderer = SDL_CreateRenderer(Video_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	if (Video_renderer == nullptr) {
		printf("Unable to create SDL renderer: %s\n", SDL_GetError());
		SDL_Quit();
		return false;
	}

	// set and clear to black
	SDL_SetRenderDrawColor(Video_renderer, 0, 0, 0, 255);
	SDL_RenderClear(Video_renderer);

	// load up the fonts that we need
	if (Video_font.load("apple2.bff") == false) {
		return false;
	}
	if (Video_inverse_font.load("apple2_inverted.bff") == false) {
		return false;
	}

	// set up a timer for flashing cursor
	Video_flash = false;
	Video_flash_timer = SDL_AddTimer(500, timer_flash_callback, nullptr);

	return true;
}

void video_shutdown()
{
	SDL_DestroyRenderer(Video_renderer);
	SDL_DestroyWindow(Video_window);
	SDL_Quit();
}

void video_render_frame(memory &mem)
{
	SDL_SetRenderDrawColor(Video_renderer, 0, 0, 0, 0);
	SDL_RenderClear(Video_renderer);
	SDL_SetRenderDrawColor(Video_renderer, 0xff, 0xff, 0xff, 0xff);

	// fow now, just run through the text memory and put out whatever is there
	uint32_t x_pixel, y_pixel;

	y_pixel = 0;
	for (auto y = 0; y < 24; y++) {
		x_pixel = 0;
		font *cur_font;
		for (auto x = 0; x < 40; x++) {
			// get normal or inverse font
			uint8_t c = mem.get_screen_char_at(y, x);
			if (c <= 0x3f) {
				cur_font = &Video_inverse_font;
			} else if ((c <= 0x7f) && (Video_flash == true)) {
				// set inverse if flashing is true
				cur_font = &Video_inverse_font;
			} else {
				cur_font = &Video_font;
			}

			// get character in memory, and then convert to ASCII.  We get character
			// value from memory and then subtract out the first character in our
			// font (as we need to be 0-based from that point).  Then we can get
			// the row/col in the bitmap sheet where the character is
			c = character_conv[c] - cur_font->m_header.m_char_offset;
			uint32_t character_row = c / cur_font->m_chars_per_row;
			uint32_t character_col = c - character_row * cur_font->m_chars_per_row;
		
			// calculate the rectangle of the character
			SDL_Rect font_rect;
			font_rect.x = character_col * cur_font->m_header.m_cell_width;
			font_rect.y = character_row * cur_font->m_header.m_cell_height;
			font_rect.w = cur_font->m_header.m_cell_width;
			font_rect.h = cur_font->m_header.m_cell_height;

			SDL_Rect screen_rect;
			screen_rect.x = x_pixel;
			screen_rect.y = y_pixel;
			screen_rect.w = cur_font->m_header.m_cell_width;
			screen_rect.h = cur_font->m_header.m_cell_height;

			// copy to screen
			SDL_RenderCopy(Video_renderer, cur_font->m_texture, &font_rect, &screen_rect);

			x_pixel += cur_font->m_header.m_cell_width;
		}
		y_pixel += cur_font->m_header.m_cell_height;
	}

	SDL_RenderPresent(Video_renderer);
}

#else  // USE_SDL

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
	move_cursor(LINES,0);
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

#endif

