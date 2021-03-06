/*

MIT License

Copyright (c) 2016-2017 Mark Allender


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

// header file for fonts
//
// Fonts used here are bitmap fonts and these have been generated using the 
// tool found at https://github.com/CodeheadUK/CBFG
//
// The TTF font used for the 40 column display is PrintChar21 and the package
// is found at http://www.kreativekorp.com/software/fonts/apple2.shtml
//
// The 80 column font is PR Number 3
//
// The ttf was converted to a bitmap file format (as defined by the CBFG tool).
//
// Making the font a class as we will have use of multiple fonts (80 col mode, etc)

#pragma once

// holds the font definition
class font {
public:
	struct header {
		uint8_t    m_id[2];
		uint32_t   m_bitmap_width;
		uint32_t   m_bitmap_height;
		uint32_t   m_cell_width;
		uint32_t   m_cell_height;
		uint8_t    m_bpp;
		uint8_t    m_char_offset;
		uint8_t    m_char_widths[256];
	};

		
	uint8_t    m_format;
	float      m_cell_u;
	float      m_cell_v;

	header        m_header;
	GLuint        m_texture_id;

	float         m_char_u[256];
	float         m_char_v[256];

	font() { }
	bool load(const char *filename);

};
