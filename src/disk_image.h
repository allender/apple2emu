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

//
// file to contain information about the disk image that was loaded
//

#pragma once

#include <string>

class disk_image
{
	enum class format_type : uint8_t {
		DOS_FORMAT,
		PRODOS_FORMAT,
		NUM_FORMATS
	};

private:
	std::string      m_filename;
	bool             m_image_dirty;
	bool             m_read_only;

	void initialize_image();

protected:
	// constants that will be useful
	static const uint32_t m_total_tracks = 35;
	static const uint32_t m_total_sectors = 16;
	static const uint32_t m_sector_bytes = 256;

	disk_image() {};

	static const uint8_t m_sector_map[static_cast<uint8_t>(format_type::NUM_FORMATS)][16];
	static const uint8_t m_prodos_block_map[8][2];

	uint8_t*         m_raw_buffer;
	size_t           m_buffer_size;
	uint8_t          m_volume_num;
	format_type      m_format;

public:

	// static function to load a disk image
	static disk_image* load_image(const char *filename);
	static const uint32_t m_dsk_image_size = 143360;
	static const uint32_t m_nib_image_size = 232960;

	virtual ~disk_image() = 0;

	// helper functions when loading disk images
	void init();
	bool save_image();
	bool unload_image();
	bool read_only() { return m_read_only; }
	const char *get_filename();

	// functions for derived classes
	virtual uint32_t read_track(const uint32_t track, uint8_t* buffer) = 0;
	virtual bool write_track(const uint32_t track, uint8_t *buffer) = 0;

	uint8_t      m_num_tracks;
};

// class for DSK image formats
class dsk_image : public disk_image
{
private:
	// tables for nibbilzing and denibbilizing
	static const uint8_t m_write_translate_table[64];
	static const uint8_t m_read_translate_table[128];

	static const uint32_t m_gap1_num_bytes = 48;
	static const uint32_t m_gap2_num_bytes = 6;
	static const uint32_t m_gap3_num_bytes = 27;

	uint32_t nibbilize_track(const int track, uint8_t *buffer);
	bool denibbilize_track(const int track, uint8_t *buffer);

public:
	dsk_image() {};
	virtual uint32_t read_track(const uint32_t track, uint8_t* buffer);
	virtual bool write_track(const uint32_t track, uint8_t *buffer);
};

class nib_image : public disk_image
{
private:
	// constants that will be useful
	static const uint32_t m_total_tracks = 35;
	static const uint32_t m_total_sectors = 16;
	static const uint32_t m_sector_bytes = 416;

public:
	virtual uint32_t read_track(const uint32_t track, uint8_t* buffer);
	virtual bool write_track(const uint32_t track, uint8_t *buffer);
};
