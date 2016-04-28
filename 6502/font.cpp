//
// implementation for fonts
//

#if defined(USE_SDL)

#include "SDL.h"
#include "SDL_image.h"
#include "6502/font.h"
#include "6502/video.h"

// load up a font defintion
bool font::load(const char *filename)
{
#if defined(USE_BFF)
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		printf("Unable to open font file %s\n", filename);
		return false;
	}

	// read in the font header
	fread(m_header.m_id, sizeof(uint8_t), sizeof(m_header.m_id), fp);
	if ((m_header.m_id[0] != 0xbf) || (m_header.m_id[1] != 0xf2)) {
		printf("Invalid font id\n");
		return false;
	}

	fread(&m_header.m_font_width, sizeof(uint32_t), 1, fp);
	fread(&m_header.m_font_height, sizeof(uint32_t), 1, fp);
	fread(&m_header.m_cell_width, sizeof(uint32_t), 1, fp);
	fread(&m_header.m_cell_height, sizeof(uint32_t), 1, fp);
	fread(&m_header.m_bpp, sizeof(uint8_t), 1, fp);
	fread(&m_header.m_char_offset, sizeof(uint8_t), 1, fp);

	for (auto i = 0; i < 256; i++) {
		fread(&m_header.m_char_widths[i], sizeof(uint8_t), 1, fp);
	}

	m_texture = SDL_CreateTexture(Video_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, m_header.m_font_width, m_header.m_font_height);
	if (m_texture == false) {
		printf("Unable to create font texture %s\n", SDL_GetError());
		return false;
	}

	// load in the pixel values to the font
	void *pixels;
	int pitch;
	if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch)) {
		printf("Unable to lock SDL texture for loading font pixels: %s\n", SDL_GetError());
		return false;
	}

	// read texture data
	fread(pixels, sizeof(uint8_t), m_header.m_font_width * m_header.m_font_height * (m_header.m_bpp / 8), fp);
	fclose(fp);

	m_chars_per_row = m_header.m_font_width / m_header.m_cell_width;
	m_column_factor = (float)m_header.m_cell_width / (float)m_header.m_font_width;
	m_row_factor = (float)m_header.m_cell_height / (float)m_header.m_font_height;
	//y_offset = CellY;

	SDL_UnlockTexture(m_texture);

#else
	m_texture = IMG_LoadTexture(Video_renderer, filename);
	if (m_texture == nullptr) {
		printf("Unable to load image %s: %s\n", filename, IMG_GetError());
		return false;
	}

	// set up font information on cells and locations for characters
	m_header.m_cell_height = 20;
	m_header.m_cell_width = 20;
	m_header

	// Figure out the cell location for each character
#endif


	return true;
}

#endif // if defined(USE_SDL)
