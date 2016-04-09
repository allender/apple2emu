#pragma once

// class for disk drive.  Represents physical drive
#include "disk_image.h"

class disk_drive {
public:
	bool        m_motor_on;
	bool        m_write_mode;
	uint8_t     m_phase_status;
	uint8_t     m_half_track_count;
	uint8_t     m_current_track;
	disk_image* m_disk_image;
	uint8_t*    m_track_data;

	disk_drive::disk_drive() {} 
	void init();
	void read();
};

void disk_init(memory &mem);
bool disk_insert(const char *disk_image_filename, const uint32_t slot);

