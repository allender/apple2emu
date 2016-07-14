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

#pragma once

// class for disk drive.  Represents physical drive
#include "disk_image.h"
#include "utils/path_utils.h"

class disk_drive {
private:
	uint8_t*     m_track_data;       // data read off of the disk put into this buffer
	uint32_t     m_track_size;       // size of the sector data
	disk_image*  m_disk_image;       // holds information about the disk image

public:
	bool        m_write_protected;  // is this disk write protected
	bool        m_motor_on;
	bool        m_write_mode;
	bool        m_track_dirty;
	uint8_t     m_phase_status;
	uint8_t     m_half_track_count;
	uint8_t     m_current_track;
	uint8_t     m_data_register;    // data register from controller which holds bytes to/from disk
	uint32_t    m_current_byte;

public:
	disk_drive():m_disk_image(nullptr) {} 
	void init(bool warm_init);
	void readwrite();
	void set_new_track(uint8_t new_track);
	bool insert_disk(const char *filename);
	uint8_t get_num_tracks();
	const char *get_mounted_filename();

};

void disk_init();
void disk_shutdown();
bool disk_insert(const char *disk_image_filename, const uint32_t slot);
const char *disk_get_mounted_filename(const uint32_t slot);

