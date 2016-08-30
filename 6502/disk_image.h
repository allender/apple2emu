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
// file to contain information about the disk image that was loaded
//

#pragma once

#include <string>

class disk_image
{

	// defines for different types of disk images we might support
	enum class image_type : uint8_t {
		DSK_IMAGE
	};

private:
	static const uint8_t m_write_translate_table[64];
	static const uint8_t m_read_translate_table[128];
	static const uint8_t m_sector_map[16];

	uint8_t*         m_raw_buffer;
	size_t           m_buffer_size;
	uint8_t          m_volume_num;
	std::string      m_filename;
	bool             m_image_dirty;
	bool             m_read_only;

	void initialize_image();
	uint32_t nibbilize_track(const int track, uint8_t *buffer);
	bool denibbilize_track(const int track, uint8_t *buffer);

public:
	// constants that will be useful
	static const uint32_t m_total_tracks = 35;
	static const uint32_t m_total_sectors = 16;
	static const uint32_t m_sector_bytes = 256;

	static const uint32_t m_gap1_num_bytes = 48;
	static const uint32_t m_gap2_num_bytes = 6;
	static const uint32_t m_gap3_num_bytes = 27;

	static const uint32_t m_nibbilized_size = 10000;

	disk_image() {}
	~disk_image();
	void init();
	bool load_image(const char *filename);
	bool save_image();
	bool unload_image();
	bool read_only() { return m_read_only; }
	uint32_t read_track(const uint32_t track, uint8_t* buffer);
	bool write_track(const uint32_t track, uint8_t *buffer);
	const char *get_filename();

	image_type   m_image_type;
	uint8_t      m_num_tracks;
};
