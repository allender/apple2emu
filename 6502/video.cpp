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

#define GLEW_STATIC

#include <GL/glew.h>
#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_image.h"
#include "6502/video.h"
#include "6502/font.h"
#include "ui/main_menu.h"

// always render to default size - SDL can scale it up
const static int Video_native_width = 280;
const static int Video_native_height = 192;
const int Video_cell_width = Video_native_width / 40;
const int Video_cell_height = Video_native_height / 24;

static float Video_scale_factor = 4.0;
static SDL_Rect Video_native_size;
static SDL_Rect Video_window_size;

// information about internally built textures
const uint8_t Num_lores_colors = 16;

GLuint Video_framebuffer;
GLuint Video_framebuffer_texture;

SDL_Window *Video_window;
SDL_GLContext Video_context;
GLuint Video_lores_texture = 0;
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
static GLubyte Lores_colors[Num_lores_colors][3] = 
{
   { 0x00, 0x00, 0x00 },                 // black
   { 0xe3, 0x1e, 0x60 },                 // red
   { 0x96, 0x4e, 0xbd },                 // dark blue
   { 0xff, 0x44, 0xfd },                 // purple
   { 0x00, 0xa3, 0x96 },                 // dark green
	{ 0x9c, 0x9c, 0x9c },                 // gray
   { 0x14, 0xcf, 0xfd },                 // medium blue
   { 0xd0, 0xce, 0xff },                 // light blue
   { 0x60, 0x72, 0x03 },                 // brown
   { 0xff, 0x6a, 0x32 },                 // orange
   { 0x9c, 0x9c, 0x9c },                 // gray
   { 0xff, 0xa0, 0xd0 },                 // pink
   { 0x14, 0xf5, 0x3c },                 // light green
   { 0xd0, 0xdd, 0x8d },                 // yellow
	{ 0x72, 0xff, 0xd0 },                 // aqua
   { 0xff, 0xff, 0xff },                 // white
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

// renders a text page (primary or secondary)
static void video_render_text_page(memory &mem)
{
	// fow now, just run through the text memory and put out whatever is there
	int x_pixel, y_pixel;
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
			
			glBindTexture(GL_TEXTURE_2D, cur_font->m_texture_id);
			glColor3f(1.0f, 1.0f, 1.0f);
			glBegin(GL_QUADS);
				glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c]); glVertex2i(x_pixel, y_pixel);
				glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c]);  glVertex2i(x_pixel + Video_cell_width, y_pixel);
				glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel + Video_cell_width, y_pixel + Video_cell_height);
				glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel, y_pixel + Video_cell_height);
			glEnd();
			glBindTexture(GL_TEXTURE_2D, 0);

			x_pixel += Video_cell_width;
		}
		y_pixel += Video_cell_height;
	}
}

// renders lores graphics mode
static void video_render_lores_mode(memory &mem)
{
	// fow now, just run through the text memory and put out whatever is there
	int x_pixel, y_pixel;
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
			glColor3ub(Lores_colors[color][0], Lores_colors[color][1], Lores_colors[color][2]);
			glBegin(GL_QUADS);
				glVertex2i(x_pixel, y_pixel);
				glVertex2i(x_pixel + Video_cell_width, y_pixel);
				glVertex2i(x_pixel + Video_cell_width, y_pixel + (Video_cell_height / 2));
				glVertex2i(x_pixel, y_pixel + (Video_cell_height / 2));
			glEnd();
			
			// render bottom cell
			color = (c & 0xf0) >> 4;
			glColor3ub(Lores_colors[color][0], Lores_colors[color][1], Lores_colors[color][2]);
			glBegin(GL_QUADS);
				glVertex2i(x_pixel, y_pixel + (Video_cell_height / 2));
				glVertex2i(x_pixel + Video_cell_width, y_pixel + (Video_cell_height / 2));
				glVertex2i(x_pixel + Video_cell_width, y_pixel + Video_cell_height);
				glVertex2i(x_pixel, y_pixel + Video_cell_height);
			glEnd();
			
			x_pixel += Video_cell_width;
		}
		y_pixel += Video_cell_height;
	}

	// deal with the rest of the display
	for (auto y = 20; y < 24; y++) {
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
			
			glBindTexture(GL_TEXTURE_2D, cur_font->m_texture_id);
			glColor3f(1.0f, 1.0f, 1.0f);
			glBegin(GL_QUADS);
				glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c]); glVertex2i(x_pixel, y_pixel);
				glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c]);  glVertex2i(x_pixel + Video_cell_width, y_pixel);
				glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel + Video_cell_width, y_pixel + Video_cell_height);
				glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel, y_pixel + Video_cell_height);
			glEnd();
			glBindTexture(GL_TEXTURE_2D, 0);

			x_pixel += Video_cell_width;
		}
		y_pixel += Video_cell_height;
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

	glColor3f(1.0f, 1.0f, 1.0f);
	glBegin(GL_POINTS);

	for (int y = 0; y < y_end; y++) {
		offset = primary?Video_hires_map[y]:Video_hires_secondary_map[y];
		for (int x = 0; x < 40; x++) {
			y_pixel = y * 8;
			for (int b = 0; b < 8; b++) {
				x_pixel = x * 7;
				uint8_t byte = mem[offset + (1024 * b) + x];
				for (int j = 0; j < 7; j++) {
					if ((byte>>j)&1) {
						glVertex2i(x_pixel, y_pixel);
					}
					x_pixel++;
				}
				y_pixel++;
			}
		}
	}
	glEnd();

	// deal with the rest of the display
	glColor3f(1.0f, 1.0f, 1.0f);
	if (mixed) {
		for (auto y = 20; y < 24; y++) {
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
				
				glBindTexture(GL_TEXTURE_2D, cur_font->m_texture_id);
				glBegin(GL_QUADS);
					glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c]); glVertex2i(x_pixel, y_pixel);
					glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c]);  glVertex2i(x_pixel + Video_cell_width, y_pixel);
					glTexCoord2f(cur_font->m_char_u[c] + cur_font->m_header.m_cell_u, cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel + Video_cell_width, y_pixel + Video_cell_height);
					glTexCoord2f(cur_font->m_char_u[c], cur_font->m_char_v[c] + cur_font->m_header.m_cell_v); glVertex2i(x_pixel, y_pixel + Video_cell_height);
				glEnd();
				glBindTexture(GL_TEXTURE_2D, 0);

				x_pixel += Video_cell_width;
			}
			y_pixel += Video_cell_height;
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
	
	// set attributes for GL
	uint32_t sdl_window_flags = SDL_WINDOW_RESIZABLE;
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	sdl_window_flags |= SDL_WINDOW_OPENGL;

	Video_window = SDL_CreateWindow("Apple2Emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(Video_native_size.w * Video_scale_factor), (int)(Video_native_size.h * Video_scale_factor), sdl_window_flags);
	if (Video_window == nullptr) {
		printf("Unable to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	// create openGL context
	if (Video_context != nullptr) {
		SDL_GL_DeleteContext(Video_context);
	}
	Video_context = SDL_GL_CreateContext(Video_window);
		
	if (Video_context == nullptr) {
		printf("Unable to create GL context: %s\n", SDL_GetError());
		return false;
	}
	glewExperimental = GL_TRUE;
	glewInit();

	// framebuffer and render to texture
	glGenFramebuffers(1, &Video_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, Video_framebuffer);

	glGenTextures(1, &Video_framebuffer_texture);
	glBindTexture(GL_TEXTURE_2D, Video_framebuffer_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Video_native_width, Video_native_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Video_framebuffer_texture, 0);
	
	GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, draw_buffers);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Unable to create framebuffer for render to texture.\n");
		return false;
	}

	if (Video_font.load("apple2.tga") == false) {
		return false;
	}
	if (Video_inverse_font.load("apple2_inverted.tga") == false) {
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

	ui_init(Video_window);

	return true;
}

void video_shutdown()
{
	SDL_GL_DeleteContext(Video_context);
	SDL_DestroyWindow(Video_window);
	SDL_Quit();
}

void video_render_frame(memory &mem)
{
	// render to the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Video_framebuffer);
	glViewport(0, 0, Video_native_width, Video_native_height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, (float)Video_native_width, (float)Video_native_height, 0.0f, 0.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);

	if (Video_mode & VIDEO_MODE_TEXT) {
		video_render_text_page(mem);
	} else if (!(Video_mode & VIDEO_MODE_HIRES)) {
		video_render_lores_mode(mem);
	} else if (Video_mode & VIDEO_MODE_HIRES) {
		video_render_hires_mode(mem);
	}

	// back to main framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Video_window_size.w, Video_window_size.h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// render texture to window.  Just render the texture to the 
	// full screen (using normalized device coordinates)
	glBindTexture(GL_TEXTURE_2D, Video_framebuffer_texture);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, -1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	ui_do_frame(Video_window);

	SDL_GL_SwapWindow(Video_window);
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

