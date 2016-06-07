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

//
// Kind of a temporary file to deal with basic text I/O for now.  Will
// use PDCurses for development
//

#if defined(USE_SDL)

#include "SDL.h"
#include "SDL_image.h"
#include "6502/video.h"
#include "6502/font.h"

// this is hires * 2 (280 * 192)
#define SCREEN_W   560
#define SCREEN_H   384

// video modes
#define VIDEO_MODE_TEXT    (1 << 0)
#define VIDEO_MODE_MIXED   (1 << 1)
#define VIDEO_MODE_PRIMARY (1 << 2)
#define VIDEO_MODE_HIRES   (1 << 3)

// information about internally built textures
const uint8_t Num_lores_colors = 16;
const uint8_t Lores_texture_size = 32;

SDL_Window *Video_window;
SDL_Renderer *Video_renderer;
SDL_Texture *Video_lores_texture;
SDL_TimerID Video_flash_timer;
bool Video_flash = false;
font Video_font, Video_inverse_font;

uint8_t Video_mode = VIDEO_MODE_TEXT;

uint16_t       Video_primary_text_map[MAX_TEXT_LINES];
uint16_t       Video_secondary_text_map[MAX_TEXT_LINES];
uint16_t       Video_hires_map[MAX_TEXT_LINES];
uint16_t       Video_hires_secondary_map[MAX_TEXT_LINES];

// values for lores colors
// see http://mrob.com/pub/xapple2/colors.html
// for values.  Good enough for a start
static uint32_t Lores_colors[Num_lores_colors] = 
{
	0x000000ff,                 // black
	0xe31e60ff,                 // red
	0x964ebdff,                 // dark blue
	0xff44fdff,                 // purple
   0x00a396ff,                 // dark green
	0x9c9c9cff,                 // gray
	0x14cffdff,                 // medium blue
	0xd0ceffff,                 // light blue
	0x607203ff,                 // brown
	0xff6a32ff,                 // orange
	0x9c9c9cff,                 // gray
   0xffa0d0ff,                 // pink
	0x14f53cff,                 // light green
   0xd0dd8dff,                 // yellow
	0x72ffd0ff,                 // aqua
	0xffffffff,                 // white
};

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

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $80	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $88	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $90	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $98	*/
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

// creates internal texture that is used for blitting lores
// colors to screen in lores mode
static bool video_create_lores_texture()
{
	Video_lores_texture = SDL_CreateTexture(Video_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Lores_texture_size * Num_lores_colors, Lores_texture_size);
	if (Video_lores_texture == nullptr) {
		printf("Unable to create internal lores texture: %s\n", SDL_GetError());
		return false;
	}

	// blit the 16 colors into the texture
	void *pixels;
	int pitch;
	if (SDL_LockTexture(Video_lores_texture, nullptr, &pixels, &pitch) != 0) {
		printf("Unable to lock lores texture:%s\n", SDL_GetError());
		return false;
	}

	for (auto i = 0; i < Num_lores_colors; i++) {
		for (auto y = 0; y < Lores_texture_size; y++) {
			for (auto x = 0; x < Lores_texture_size; x++) {
				((uint32_t *)(pixels))[(y * (Lores_texture_size * Num_lores_colors)) + (i * Lores_texture_size) + x] = Lores_colors[i];
			}
		}
	}

	// unlock the texture
	SDL_UnlockTexture(Video_lores_texture);

	return true;
}

// renders a text page (primary or secondary)
static void video_render_text_page(memory &mem)
{
	// fow now, just run through the text memory and put out whatever is there
	uint32_t x_pixel, y_pixel;

	bool primary = (Video_mode & VIDEO_MODE_PRIMARY) ? true : false;

	y_pixel = 0;
	for (auto y = 0; y < 24; y++) {
		x_pixel = 0;
		font *cur_font;
		for (auto x = 0; x < 40; x++) {
			uint16_t addr = primary ? Video_primary_text_map[y] + x : Video_secondary_text_map[y] + x;  // m_screen_map[row] + col;
			uint8_t c = mem[addr];

			// get normal or inverse font
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

			SDL_Rect screen_rect;
			screen_rect.x = x_pixel;
			screen_rect.y = y_pixel;
			screen_rect.w = cur_font->m_header.m_cell_width;
			screen_rect.h = cur_font->m_header.m_cell_height;

			// copy to screen
			SDL_RenderCopy(Video_renderer, cur_font->m_texture, &cur_font->m_char_rects[c], &screen_rect);

			x_pixel += cur_font->m_header.m_cell_width;
		}
		y_pixel += cur_font->m_header.m_cell_height;
	}
}

// renders lores graphics mode
static void video_render_lores_mode(memory &mem)
{
	// fow now, just run through the text memory and put out whatever is there
	uint32_t x_pixel, y_pixel;

	bool primary = (Video_mode & VIDEO_MODE_PRIMARY) ? true : false;

	y_pixel = 0;
	for (auto y = 0; y < 20; y++) {
		x_pixel = 0;
		for (auto x = 0; x < 40; x++) {
			// get the 2 nibble value of the color.  Blit the corresponding colors
			// to the screen
			uint16_t addr = primary ? Video_primary_text_map[y] + x : Video_secondary_text_map[y] + x;  // m_screen_map[row] + col;
			uint8_t c = mem[addr];

			// render top cell
			uint8_t color = c & 0x0f;
			
			SDL_Rect source_rect;
			source_rect.x = (color * Lores_texture_size);
			source_rect.y = 0;
			source_rect.w = 16;
			source_rect.h = 14;
			
			SDL_Rect screen_rect;
			screen_rect.x = x_pixel;
			screen_rect.y = y_pixel;
			screen_rect.w = 16;
			screen_rect.h = 14;
			
			SDL_RenderCopy(Video_renderer, Video_lores_texture, &source_rect, &screen_rect);

			// render bottom cell
			color = (c & 0xf0) >> 4;
			
			source_rect.x = (color * Lores_texture_size);
			source_rect.y = 0;
			source_rect.w = 16;
			source_rect.h = 7;
			
			screen_rect.x = x_pixel;
			screen_rect.y = y_pixel+8;
			screen_rect.w = 16;
			screen_rect.h = 7;
			
			SDL_RenderCopy(Video_renderer, Video_lores_texture, &source_rect, &screen_rect);

			x_pixel += 14;
		}
		y_pixel += 16;
	}

	// deal with the rest of the display
	for (auto y = 20; y < 24; y++) {
		font *cur_font;
		x_pixel = 0;
		for (auto x = 0; x < 40; x++) {
			// get normal or inverse font
			uint16_t addr = primary ? Video_primary_text_map[y] + x : Video_secondary_text_map[y] + x;  // m_screen_map[row] + col;
			uint8_t c = mem[addr];
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

			SDL_Rect screen_rect;
			screen_rect.x = x_pixel;
			screen_rect.y = y_pixel;
			screen_rect.w = cur_font->m_header.m_cell_width;
			screen_rect.h = cur_font->m_header.m_cell_height;

			// copy to screen
			SDL_RenderCopy(Video_renderer, cur_font->m_texture, &cur_font->m_char_rects[c], &screen_rect);

			x_pixel += cur_font->m_header.m_cell_width;
		}
		y_pixel += cur_font->m_header.m_cell_height;
	}
}

static void video_render_hires_mode(memory &mem)
{
	bool primary = (Video_mode & VIDEO_MODE_PRIMARY) ? true : false;
	uint16_t offset;

	bool mixed = (Video_mode & VIDEO_MODE_MIXED) ? true : false;
	int y_end = 0;
	if (mixed == true) {
		y_end = 20;
	} else {
		y_end = 24;
	}

	// do the stupid easy thing first.  Just plow through
	// the memory and put the pixels on the screen
	int x_pixel;
	int y_pixel;
	for (int y = 0; y < y_end; y++) {
		offset = primary?Video_hires_map[y]:Video_hires_secondary_map[y];
		for (int x = 0; x < 40; x++) {
			y_pixel = y * 8 * 2;
			for (int b = 0; b < 8; b++) {
				x_pixel = x * 7 * 2;
				uint8_t byte = mem[offset + (1024 * b) + x];
				for (int j = 0; j < 7; j++) {
					if ((byte>>j)&1) {
						SDL_Rect rect;

						rect.x = x_pixel;
						rect.y = y_pixel;
						rect.w = 2;
						rect.h = 2;
						SDL_RenderDrawRect(Video_renderer, &rect);
					}
					x_pixel += 2;
				}
				y_pixel += 2;
			}
		}
	}

	// deal with the rest of the display
	if (mixed) {
		y_pixel = 320;  // 320 is magic number -- need to get rid of this.
		for (auto y = 20; y < 24; y++) {
			font *cur_font;
			x_pixel = 0;
			for (auto x = 0; x < 40; x++) {
				// get normal or inverse font
				uint16_t addr = primary ? Video_primary_text_map[y] + x : Video_secondary_text_map[y] + x;  // m_screen_map[row] + col;
				uint8_t c = mem[addr];
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

				SDL_Rect screen_rect;
				screen_rect.x = x_pixel;
				screen_rect.y = y_pixel;
				screen_rect.w = cur_font->m_header.m_cell_width;
				screen_rect.h = cur_font->m_header.m_cell_height;

				// copy to screen
				SDL_RenderCopy(Video_renderer, cur_font->m_texture, &cur_font->m_char_rects[c], &screen_rect);

				x_pixel += cur_font->m_header.m_cell_width;
			}
			y_pixel += cur_font->m_header.m_cell_height;
		}
	}
}

static uint8_t video_read_handler(uint16_t addr)
{
	uint8_t a = addr & 0xff;

	// switch based on the address to set the video modes
	switch (a) {
	case 0x50:
		Video_mode &= ~VIDEO_MODE_TEXT;
		break;
	case 0x51:
		Video_mode |= VIDEO_MODE_TEXT;
		break;
	case 0x52:
		Video_mode &= ~VIDEO_MODE_MIXED;
		break;
	case 0x53:
		Video_mode |= VIDEO_MODE_MIXED;
		break;
	case 0x54:
		Video_mode |= VIDEO_MODE_PRIMARY;
		break;
	case 0x55:
		Video_mode &= ~VIDEO_MODE_PRIMARY;
		break;
	case 0x56:
		Video_mode &= ~VIDEO_MODE_HIRES;
		break;
	case 0x57:
		Video_mode |= VIDEO_MODE_HIRES;
		break;
	}

	return 0;
}

static void video_write_handler(uint16_t addr, uint8_t value) 
{
	uint8_t a = addr & 0xff;

	// switch based on the address to set the video modes
	switch (a) {
	case 0x50:
		Video_mode &= ~VIDEO_MODE_TEXT;
		break;
	case 0x51:
		Video_mode |= VIDEO_MODE_TEXT;
		break;
	case 0x52:
		Video_mode &= ~VIDEO_MODE_MIXED;
		break;
	case 0x53:
		Video_mode |= VIDEO_MODE_MIXED;
		break;
	case 0x54:
		Video_mode |= VIDEO_MODE_PRIMARY;
		break;
	case 0x55:
		Video_mode &= ~VIDEO_MODE_PRIMARY;
		break;
	case 0x56:
		Video_mode &= ~VIDEO_MODE_HIRES;
		break;
	case 0x57:
		Video_mode |= VIDEO_MODE_HIRES;
		break;
	}
}

// intialize the SDL system
bool video_init(memory &mem)
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

	for (auto i = 0x50; i <= 0x57 ; i++) {
		mem.register_c000_handler(i, video_read_handler, video_write_handler);
	}

	// set and clear to black
	SDL_SetRenderDrawColor(Video_renderer, 0, 0, 0, 255);
	SDL_RenderClear(Video_renderer);

	// load up the fonts that we need
#if defined(USE_BFF)
	if (Video_font.load("apple2.bff") == false) {
		return false;
	}
	if (Video_inverse_font.load("apple2_inverted.bff") == false) {
		return false;
	}
#else
	if (Video_font.load("apple2.tga") == false) {
		return false;
	}
	if (Video_inverse_font.load("apple2_inverted.tga") == false) {
		return false;
	}
#endif

	// create texture that contains source for lores colors
	if (video_create_lores_texture() == false) {
		return false;
	}

	// set up a timer for flashing cursor
	Video_flash = false;
	Video_flash_timer = SDL_AddTimer(250, timer_flash_callback, nullptr);

	// set up screen map for video output.  This per/row
	// table gets starting memory address for that row of text
	//
	// algorithm here pulled from Apple 2 Monitors Peeled, pg 15
	for (int i = 0; i < MAX_TEXT_LINES; i++) {
		Video_primary_text_map[i] = 1024 + 256 * ((i/2) % 4)+(128*(i%2))+40*((i/8)%4);
		Video_secondary_text_map[i] = 2048 + 256 * ((i/2) % 4)+(128*(i%2))+40*((i/8)%4);

		// set up the hires map at the same time -- same number of lines just offset by fixed amount
		Video_hires_map[i] = 0x1c00 + Video_primary_text_map[i];
		Video_hires_secondary_map[i] = Video_hires_map[i] + 0x2000;
	}

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

	if (Video_mode & VIDEO_MODE_TEXT) {
		video_render_text_page(mem);
	} else if (!(Video_mode & VIDEO_MODE_HIRES)) {
		video_render_lores_mode(mem);
	} else if (Video_mode & VIDEO_MODE_HIRES) {
		video_render_hires_mode(mem);
	}

	//SDL_Rect rect;
	//rect.x = 0;
	//rect.y = 0;
	//rect.w = Lores_texture_size * Num_lores_colors;
	//rect.h = Lores_texture_size;
	//SDL_RenderCopy(Video_renderer, Video_lores_texture, nullptr, &rect);

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

