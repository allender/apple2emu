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

#include "SDL.h"
#include "SDL_image.h"
#include "6502/video.h"
#include "6502/font.h"

// always render to default size - SDL can scale it up
const static int Video_native_width = 280;
const static int Video_native_height = 192;
static float Video_scale_factor = 2.0;
static SDL_Rect Video_native_size;
static SDL_Rect Video_window_size;
bool Video_resize = true;

// information about internally built textures
const uint8_t Num_lores_colors = 16;
const uint8_t Lores_texture_size = 32;
const uint8_t Num_hires_patterns = 127;
const uint8_t Hires_texture_width = 7;

SDL_Window *Video_window;
SDL_Renderer *Video_renderer;
SDL_Texture *Video_backbuffer;
SDL_Texture *Video_lores_texture;
SDL_Texture *Video_hires_texture;
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
	if (Video_lores_texture != nullptr) {
		SDL_DestroyTexture(Video_lores_texture);
	}
	Video_lores_texture = SDL_CreateTexture(Video_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Lores_texture_size * Num_lores_colors, Lores_texture_size);
	if (Video_lores_texture == nullptr) {
		printf("Unable to create internal lores texture: %s\n", SDL_GetError());
		return false;
	}

	// blit the 16 colors into the texture
	void *pixels;
	int pitch;
	if (SDL_LockTexture(Video_lores_texture, nullptr, &pixels, &pitch) != 0) {
		printf("Unable to lock lores texture: %s\n", SDL_GetError());
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

// create internal texture which will be used for blitting hires pixel
// patterns in high res mode
static bool video_create_hires_textures()
{
	if (Video_hires_texture != nullptr) {
		SDL_DestroyTexture(Video_hires_texture);
	}
	Video_hires_texture = SDL_CreateTexture(Video_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, Hires_texture_width, Num_hires_patterns);
	if (Video_hires_texture == nullptr) {
		printf("Unable to create internal lores texture: %s\n", SDL_GetError());
		return false;
	}

	// blit the 16 colors into the texture
	void *pixels;
	int pitch;
	if (SDL_LockTexture(Video_hires_texture, nullptr, &pixels, &pitch) != 0) {
		printf("Unable to lock hires texture: %s\n", SDL_GetError());
		return false;
	}

	// create texture that contains pixel patterns for black/white display.  Note that there are only
	// 127 patterns because the high bit is used to determine color of pixels (when running in color mode).
	// in monochrome mode, these are all just white pixels
	for (auto y = 0; y < Num_hires_patterns; y++) {
		uint32_t *src = &((uint32_t *)(pixels))[y * Hires_texture_width];
		for (auto x = 0; x <= Hires_texture_width; x++) {
			if ((y>>x)&1) {
				*src++ = 0xffffffff;
			} else {
				*src++ = 0;
			}
			//((uint32_t *)(pixels))[(y * pitch) + 7 - x] = ((y >> x)&1)?0xffffffff:0;
		}
	}

	// unlock the texture
	SDL_UnlockTexture(Video_hires_texture);

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
			screen_rect.w = cur_font->m_header.m_cell_width/2;
			screen_rect.h = cur_font->m_header.m_cell_height/2;

			// copy to screen
			SDL_RenderCopy(Video_renderer, cur_font->m_texture, &cur_font->m_char_rects[c], &screen_rect);

			x_pixel += cur_font->m_header.m_cell_width / 2;
		}
		y_pixel += cur_font->m_header.m_cell_height / 2;
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
			screen_rect.w = cur_font->m_header.m_cell_width/2;
			screen_rect.h = cur_font->m_header.m_cell_height/2;

			// copy to screen
			SDL_RenderCopy(Video_renderer, cur_font->m_texture, &cur_font->m_char_rects[c], &screen_rect);

			x_pixel += (cur_font->m_header.m_cell_width/2);
		}
		y_pixel += (cur_font->m_header.m_cell_height/2);
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
		y_pixel = y * 8;
		for (int x = 0; x < 40; x++) {
			x_pixel = x * 7;
			for (int b = 0; b < 8; b++) {
				// lookup the hires texture pixels for this value
				uint8_t byte = mem[offset + (1024 * b) + x];
				SDL_Rect source_rect, screen_rect;

				screen_rect.x = x_pixel;
				screen_rect.y = y_pixel+b;
				screen_rect.w = 7;
				screen_rect.h = 1;

				source_rect.x = 0;
				source_rect.y = byte & 0x7f;
				source_rect.w = 7;
				source_rect.h = 1;
				SDL_RenderCopy(Video_renderer, Video_hires_texture, &source_rect, &screen_rect);
			}
		}
			

		//for (int x = 0; x < 40; x++) {
		//	y_pixel = y * 8 * 2;
		//	for (int b = 0; b < 8; b++) {
		//		x_pixel = x * 7 * 2;
		//		uint8_t byte = mem[offset + (1024 * b) + x];
		//		for (int j = 0; j < 7; j++) {
		//			if ((byte>>j)&1) {
		//				SDL_Rect rect;

		//				rect.x = x_pixel;
		//				rect.y = y_pixel;
		//				rect.w = 2;
		//				rect.h = 2;
		//				SDL_RenderDrawRect(Video_renderer, &rect);
		//			}
		//			x_pixel += 2;
		//		}
		//		y_pixel += 2;
		//	}
		//}
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

bool video_create()
{
	// set the rect for the window itself
	Video_window_size.x = 0;
	Video_window_size.y = 0;
	Video_window_size.w = (int)(Video_scale_factor * Video_native_size.w);
	Video_window_size.h = (int)(Video_scale_factor * Video_native_size.h);

	// create SDL window
	if (Video_window != nullptr) {
		SDL_DestroyWindow(Video_window);
	}
	Video_window = SDL_CreateWindow("Apple2Emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(Video_native_size.w * Video_scale_factor), (int)(Video_native_size.h * Video_scale_factor), SDL_WINDOW_SHOWN);
	if (Video_window == nullptr) {
		printf("Unable to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	// create SDL renderer
	if (Video_renderer != nullptr) {
		SDL_DestroyRenderer(Video_renderer);
	}
	Video_renderer = SDL_CreateRenderer(Video_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	if (Video_renderer == nullptr) {
		printf("Unable to create SDL renderer: %s\n", SDL_GetError());
		return false;
	}

	// create  backbuffer texture.  We render to this texture then scale to the window
	if (Video_backbuffer != nullptr) {
		SDL_DestroyTexture(Video_backbuffer);
	}
	SDL_SetRenderTarget(Video_renderer, nullptr);
	Video_backbuffer = SDL_CreateTexture(Video_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Video_native_size.w, Video_native_size.h);

	if (Video_font.load("apple2.tga") == false) {
		return false;
	}
	if (Video_inverse_font.load("apple2_inverted.tga") == false) {
		return false;
	}
	if (video_create_lores_texture() == false) {
		return false;
	}
	if (video_create_hires_textures() == false) {
		return false;
	}

	return true;
}

// intialize the SDL system
bool video_init(memory &mem)
{
	Video_native_size.x = 0;
	Video_native_size.y = 0;
	Video_native_size.w = Video_native_width;
	Video_native_size.h = Video_native_height;

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return false;
	}
	if (video_create() == false) {
		return false;
	}
		
	for (auto i = 0x50; i <= 0x57 ; i++) {
		mem.register_c000_handler(i, video_read_handler, video_write_handler);
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
	SDL_SetRenderTarget(Video_renderer, Video_backbuffer);
	SDL_RenderClear(Video_renderer);

	if (Video_mode & VIDEO_MODE_TEXT) {
		video_render_text_page(mem);
	} else if (!(Video_mode & VIDEO_MODE_HIRES)) {
		video_render_lores_mode(mem);
	} else if (Video_mode & VIDEO_MODE_HIRES) {
		video_render_hires_mode(mem);
	}

	// for rendering, show the buffer that we've been rendering to, which will
	// scale to the current windows size
	SDL_SetRenderTarget(Video_renderer, nullptr);
	SDL_RenderCopy(Video_renderer, Video_backbuffer, &Video_native_size, &Video_window_size);
	SDL_RenderPresent(Video_renderer);
}

// called when window changes size - adjust scaling parameters
// so that we still render properly
void video_resize(bool scale_up)
{
	if (scale_up == true) {
		Video_scale_factor += 0.5f;
	} else {
		Video_scale_factor -= 0.5f;
	}

	// create window and textures
	video_create();
}

