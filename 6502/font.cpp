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

#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_image.h"
#include "6502/font.h"
#include "6502/video.h"

// load up a font defintion
bool font::load(const char *filename)
{
	if (m_surface != nullptr) {
		SDL_FreeSurface(m_surface);
	}

	m_surface = IMG_Load(filename);
	if (m_surface == nullptr) {
		printf("Unable to load image %s: %s\n", filename, IMG_GetError());
		return false;
	}

	// load the pixels into a GL texture
	glGenTextures(1, &m_texture_id);
	glBindTexture(GL_TEXTURE_2D, m_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_surface->w, m_surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, m_surface->pixels);

	// set up font information on cells and locations for characters
	m_header.m_font_width = m_surface->w;
	m_header.m_font_height = m_surface->h;
	m_header.m_format = m_surface->format->format;

	// Figure out the cell location for each character.  The offset is the ascii offset to the
	// first character in the font.  The cell widths are hard coded because this is how
	// the texture was exported from CBFG
	m_header.m_char_offset = 32;
	m_header.m_cell_height = 16;
	m_header.m_cell_width = 14;

	// uv widths and heights for the cells - they are all fixed so this
	// is a fixed value
	m_header.m_cell_u = (float)m_header.m_cell_width / (float)m_header.m_font_width;
	m_header.m_cell_v = (float)m_header.m_cell_height / (float)m_header.m_font_height;

	uint32_t chars_per_row = m_header.m_font_width / m_header.m_cell_width;

	// calculate rects for characters in the font.  These things here will be 0
	// based as we need to start with the first character in the bitmap sheet.
	for (auto i = 0; i < 128; i++) {
		uint32_t character_row = i / chars_per_row;
		uint32_t character_col = i - character_row * chars_per_row;

		m_char_u[i] = character_col * m_header.m_cell_u;
		m_char_v[i] = character_row * m_header.m_cell_v;
	}

	return true;
}
