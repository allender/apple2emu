
#include "SDL.h"

// header file for fonts
//
// Fonts used here are bitmap fonts and these have been generated using the 
// tool found at https://github.com/CodeheadUK/CBFG
//
// The TTF font used for the 40 column display is PrintChar21 and the package is found
// at http://www.kreativekorp.com/software/fonts/apple2.shtml
//
// The ttf was converted to a bitmap file format (as defined by the CBFG tool).
//
// Making the font a class as we will have use of multiple fonts (80 col mode, etc)

#pragma once

#define USE_BFF

// holds the font definition
class font {
public:
	struct header {
		uint8_t    m_id[2];
		uint32_t   m_font_width;
		uint32_t   m_font_height;
		uint32_t   m_cell_width;
		uint32_t   m_cell_height;
		uint8_t    m_bpp;
		uint8_t    m_char_offset;
		uint8_t    m_char_widths[256];
	};

	header        m_header;
	SDL_Texture*  m_texture;

	int32_t       m_chars_per_row;
	float         m_column_factor;
	float         m_row_factor;

	font() { }
	bool load(const char *filename);

};