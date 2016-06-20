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
#include "SDL_image.h"
#include "6502/font.h"
#include "6502/video.h"

// load up a font defintion
bool font::load(const char *filename)
{
	if (m_texture != nullptr) {
		SDL_DestroyTexture(m_texture);
	}

	m_texture = IMG_LoadTexture(Video_renderer, filename);
	if (m_texture == nullptr) {
		printf("Unable to load image %s: %s\n", filename, IMG_GetError());
		return false;
	}

	// set up font information on cells and locations for characters
	if (SDL_QueryTexture(m_texture, &m_header.m_format, nullptr, &m_header.m_font_width, &m_header.m_font_height) != 0) {
		printf("Unable to query texture: %s\n", SDL_GetError());
	}

	// Figure out the cell location for each character.  The offset is the ascii offset to the
	// first character in the font.  The cell widths are hard coded because this is how
	// the texture was exported from CBFG
	m_header.m_char_offset = 32;
	m_header.m_cell_height = 16;
	m_header.m_cell_width = 14;

	m_chars_per_row = m_header.m_font_width / m_header.m_cell_width;
	m_column_factor = (float)m_header.m_cell_width / (float)m_header.m_font_width;
	m_row_factor = (float)m_header.m_cell_height / (float)m_header.m_font_height;

	// calculate rects for characters in the font.  These things here will be 0
	// based as we need to start with the first character in the bitmap sheet.
	for (auto i = 0; i < 128; i++) {
		uint32_t character_row = i / m_chars_per_row;
		uint32_t character_col = i - character_row * m_chars_per_row;
	
		// calculate the rectangle of the character
		m_char_rects[i].h = m_header.m_cell_height;
		m_char_rects[i].w = m_header.m_cell_width;
		m_char_rects[i].x = character_col * m_header.m_cell_width;
		m_char_rects[i].y = character_row * m_header.m_cell_height;
	}

	return true;
}
