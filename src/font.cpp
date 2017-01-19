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
// implementation for fonts
//

#include <stdio.h>

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_image.h"

#include "apple2emu_defs.h"
#include "font.h"
#include "video.h"

// load up a font defintion
bool font::load(const char *filename)
{
	FILE *fp;
	char path[1024]; // maybe should fix this

	strcpy(path, "fonts/");
	strcat(path, filename);

	fp = fopen(path, "rb");
	if (fp == nullptr) {
		return false;
	}
	fread(m_header.m_id, sizeof(uint8_t), 2, fp);
	fread(&m_header.m_bitmap_width, sizeof(m_header.m_bitmap_width), 1, fp);
	fread(&m_header.m_bitmap_height, sizeof(m_header.m_bitmap_height), 1, fp);
	fread(&m_header.m_cell_width, sizeof(m_header.m_cell_width), 1, fp);
	fread(&m_header.m_cell_height, sizeof(m_header.m_cell_height), 1, fp);
	fread(&m_header.m_bpp, sizeof(m_header.m_bpp), 1, fp);
	fread(&m_header.m_char_offset, sizeof(m_header.m_char_offset), 1, fp);
	fread(&m_header.m_char_widths, sizeof(uint8_t), 256, fp);

	// now read the pixels
	int num_pixels = m_header.m_bitmap_width * m_header.m_bitmap_height * (m_header.m_bpp / 8);
	uint8_t *pixels = new uint8_t[num_pixels];
	fread(pixels, num_pixels, 1, fp);
	fclose(fp);

	ASSERT((m_header.m_id[0] == 0xbf) && (m_header.m_id[1] == 0xf2));

	// load the pixels into a GL texture
	glGenTextures(1, &m_texture_id);
	glBindTexture(GL_TEXTURE_2D, m_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_header.m_bitmap_width, m_header.m_bitmap_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	// set up font information on cells and locations for characters
	//
	// I used CBFG to generate the fonts.  it was a TTF to bitmap font generator
	// that was totally suited for what I needed.  I've hard coded values here
	// because honestly I don't need to do anything different.
	//
	// http://www.codehead.co.uk/cbfg/
	//

	// uv widths and heights for the cells - they are all fixed so this
	// is a fixed value
	m_cell_u = (float)m_header.m_cell_width / (float)m_header.m_bitmap_width;
	m_cell_v = (float)m_header.m_cell_height / (float)m_header.m_bitmap_height;

	uint32_t chars_per_row = m_header.m_bitmap_width / m_header.m_cell_width;

	// calculate rects for characters in the font.  These things here will be 0
	// based as we need to start with the first character in the bitmap sheet.
	for (auto i = 0; i < 128; i++) {
		uint32_t character_row = i / chars_per_row;
		uint32_t character_col = i - character_row * chars_per_row;

		m_char_u[i] = character_col * m_cell_u;
		m_char_v[i] = character_row * m_cell_v;
	}

	// I had this in here when getting 80 column mode running because
	// we had to have widths of 7, but I think that with the new
	// way of loading fonts and rendering (to 560x380), this isn't necessary
	// anymore for some reason
	if (m_header.m_cell_width == 8) {
		m_cell_u -= 1.0f / m_header.m_bitmap_width;
	}

	return true;
}
