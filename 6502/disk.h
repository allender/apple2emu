#pragma once

// class for disk drive.  Represents physical drive
#include "disk_image.h"

class disk_drive {
private:
	uint8_t*     m_track_data;      // data read off of the disk put into this buffer
	uint32_t     m_track_size;      // size of the sector data

public:
	bool        m_motor_on;
	bool        m_write_mode;
	uint8_t     m_phase_status;
	uint8_t     m_half_track_count;
	uint8_t     m_current_track;
	disk_image* m_disk_image;       // holds information about the disk image
	uint8_t     m_data_buffer;
	uint32_t    m_current_byte;

	disk_drive::disk_drive() {} 
	void init();
	void readwrite();
	void set_new_track(uint8_t new_track);
};

void disk_init(memory &mem);
bool disk_insert(const char *disk_image_filename, const uint32_t slot);

