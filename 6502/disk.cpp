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
// source file for disk operations/

#include <algorithm>
#include <iomanip>
#include "disk.h"
#include "memory.h"

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

//#define LOG_DISK

// I NEED TO FIGURE OUT WHAT THIS DEFINE WORKS
#define NIBBLES_PER_TRACK 0x1A00

static disk_drive drive_1;
static disk_drive drive_2;
static disk_drive *Current_drive;

void disk_drive::init(bool warm_init)
{
	m_motor_on = false;
	m_write_mode = false;
	m_track_dirty = false;
	m_data_register = 0;
	m_current_byte = 0;
	if (m_track_data != nullptr) {
		delete[] m_track_data;
	}
	m_track_data = nullptr;
	m_track_size = 0;

	// for warm initialization, we don't do certain things
	// like move track and phase motors
	if (warm_init == false) {
		m_phase_status = 0;
		m_half_track_count = 0;
		m_current_track = 0;
	}
}

void disk_drive::readwrite()
{
	if (m_track_data == nullptr) {
		m_track_data = new uint8_t[disk_image::m_nibbilized_size];
		if (m_track_data == nullptr) {
			return;
		}
	}

	// read the data out of the disk image into the track image
	if (m_track_size == 0) {
#if defined(LOG_DISK)
		LOG(INFO) << "track $" << std::setw(2) << std::setfill('0') << std::setbase(16) << (uint32_t)Current_drive->m_current_track << "  read";
#endif
	 	m_track_size = m_disk_image->read_track(Current_drive->m_current_track, m_track_data);
		m_track_dirty = false;
		m_current_byte = 0;
	}

	if (m_write_mode == false) {
		// this is read mode
		m_data_register = m_track_data[m_current_byte];
#if defined(LOG_DISK)
		LOG(INFO) << "Read: " << std::setw(4) << std::setfill(' ') << std::setbase(16) << m_current_byte << " " <<
std::setw(2) << std::setbase(16) << (uint32_t)m_data_register;
#endif
	} else {
		// when writing, here we will just apply the byte to the track
		// data.  It will get written when we change tracks or eject
		// the disk
		m_track_data[m_current_byte] = m_data_register;
		m_track_dirty = true;
	}
	m_current_byte++;
	if (m_current_byte == m_track_size) {
		m_current_byte = 0;
	}
}

void disk_drive::set_new_track(const uint8_t track)
{
	// write out old track if we are switching to a new track.
	if (m_track_dirty == true && m_current_track != track && m_track_data != nullptr) {
		m_disk_image->write_track(Current_drive->m_current_track, m_track_data);
	}
	m_current_track = track;

	// get rid of the old sector data.  Need to do this a better way to avoid
	// all the new/delete calls
	delete[] m_track_data;
	m_track_data = nullptr;
	m_track_size = 0;
}

bool disk_drive::insert_disk(const char *filename)
{
	if (m_disk_image != nullptr) {
		delete m_disk_image;
		m_disk_image = nullptr;
	}
	init(true);
	m_disk_image = new disk_image();
	m_disk_image->init();
	m_disk_image->load_image(filename);
	return true;
}

const char *disk_drive::get_mounted_filename()
{
	if (m_disk_image != nullptr) {
		return m_disk_image->get_filename();
	}
	return nullptr;
}

uint8_t disk_drive::get_num_tracks()
{
	if (m_disk_image != nullptr) {
		return m_disk_image->m_num_tracks;
	}
	return 0;
}

// inserts a disk image into the given slot
bool disk_insert(const char *disk_image_filename, const uint32_t slot)
{
	if (slot == 1) {
		drive_1.insert_disk(disk_image_filename);
	} else {
		drive_2.insert_disk(disk_image_filename);
	}
	return true;
}

uint8_t read_handler(uint16_t addr)
{
	uint8_t action = (addr & 0x000f);

	// 0x0 - 0x7 control the stepper motors
	switch (action) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	{
		if ((addr & 0x000f) <= 0x7) {
			uint8_t phase = (addr >> 1) & 3;	   // gets which motor, 0, 1, 2, or 3

			if (addr & 1) {
				Current_drive->m_phase_status |= (1 << phase);
			} else {
				Current_drive->m_phase_status &= ~(1 << phase);
			}

			// step in (increment) to new track.  phase 1, then phase 2
			// activated from even numbered track.  Phase 3 then phase 0
			// activated from odd numbered track.
			//
			// step out (decrement) to new track.  Phase 3, then phase
			// 2 from even numbered track.  Phase 1, then phase 0
			// from odd numbered track.
			int dir = 0;
			if (Current_drive->m_phase_status & (1 << ((Current_drive->m_half_track_count + 1) & 3))) {
				dir = 1;
			}
			if (Current_drive->m_phase_status & (1 << ((Current_drive->m_half_track_count + 3) & 3))) {
				dir = -1;
			}

			if (dir != 0) {
				Current_drive->m_half_track_count = std::max(0, std::min(79, Current_drive->m_half_track_count + dir));
				auto new_track = std::min(Current_drive->get_num_tracks() - 1, Current_drive->m_half_track_count >> 1);
				if (new_track != Current_drive->m_current_track) {
					Current_drive->set_new_track(new_track);
				}
			}
#if defined(LOG_DISK)
			LOG(INFO) << "phases: " << std::setw(1) << ((Current_drive->m_phase_status >> 3) & 0x1)
				<< std::setw(1) << ((Current_drive->m_phase_status >> 2) & 0x1)
				<< std::setw(1) << ((Current_drive->m_phase_status >> 1) & 0x1)
				<< std::setw(1) << (Current_drive->m_phase_status & 0x1)
				<< " track: " << std::setw(2) << (int)Current_drive->m_half_track_count
				<< " dir: " << std::setw(1) << dir
				<< " addr: " << std::setbase(16) << (addr & 0xff);
#endif
		}
	}
	break;

	case 0x8:
	case 0x9:
		// turn the motor on or off
		Current_drive->m_motor_on = (addr & 1);
		break;

	case 0xa:
	case 0xb:
		// pick a specific drive
		if (((addr & 0xf) - 0xa) == 0) {
			Current_drive = &drive_1;
		} else {
			Current_drive = &drive_2;
		}
		//LOG(INFO) << "Select drive: " << ((addr & 0xf) - 0x9);
		break;

	case 0xc:
		Current_drive->readwrite();
		break;
		
	case 0xd:
		// check write-protect status. If the drive is write protected,
		// we would or a high bit into the data register as this bit
		// is used to determine write protect status.  Removing
		// the high bit forces the QA bit low which enables writing
		if (Current_drive->m_disk_image->read_only() == true) {
			Current_drive->m_data_register |= 0x80;
		} else {
			Current_drive->m_data_register &= 0x7f;
		}
		break;

	case 0xe:
		Current_drive->m_write_mode = false;
		break;

	case 0xf:
		Current_drive->m_write_mode = true;
		break;
	}

	if (!(addr & 0x1)) {
		return Current_drive->m_data_register;
	} else {
		return 0;
	}

}

void write_handler(uint16_t addr, uint8_t val)
{
	read_handler(addr);
	Current_drive->m_data_register = val;
}

// initialize the disk system
void disk_init()
{
	memory_register_slot_handler(6, read_handler, write_handler);
	drive_1.init(false);
	drive_2.init(false);
	Current_drive = &drive_1;
}

void disk_shutdown()
{
	if (drive_1.m_disk_image != nullptr) {
		delete drive_1.m_disk_image;
	}
	if (drive_2.m_disk_image != nullptr) {
		delete drive_2.m_disk_image;
	}
}

// return the filename of the mounted disk in the given slot
const char *disk_get_mounted_filename(const uint32_t slot)
{
	if (slot == 1) {
		return drive_1.get_mounted_filename();
	} else if (slot == 2) {
		return drive_2.get_mounted_filename();
	}
	return nullptr;
}
