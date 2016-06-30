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

void disk_drive::init()
{
	m_current_track = 0;
	m_half_track_count = 0;
	m_phase_status = 0;
	m_motor_on = false;
	m_track_data = nullptr;
	m_current_byte = 0;
	m_data_buffer = 0;
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
		LOG(INFO) << "track $" << std::setw(2) << std::setfill('0') << std::setbase(16) << (uint32_t)drive_1.m_current_track << "  read";
#endif
	 	m_track_size = m_disk_image->read_track(drive_1.m_current_track, m_track_data);
		m_current_byte = 0;
	}

	if (m_write_mode == false) {
		// this is read mode
		m_data_buffer = m_track_data[m_current_byte];
#if defined(LOG_DISK)
		LOG(INFO) << "Read: " << std::setw(4) << std::setfill(' ') << std::setbase(16) << m_current_byte << " " <<
std::setw(2) << std::setbase(16) << (uint32_t)m_data_buffer;
#endif
	} else {
	// this is write mode
	}
	m_current_byte++;
	if (m_current_byte == m_track_size) {
		m_current_byte = 0;
	}
}

void disk_drive::set_new_track(const uint8_t track)
{
	m_current_track = track;

	// get rid of the old sector data.  Need to do this a better way to avoid
	// all the new/delete calls
	delete[] m_track_data;
	m_track_data = nullptr;
	m_track_size = 0;
}

// inserts a disk image into the given slot
bool disk_insert(const char *disk_image_filename, const uint32_t slot)
{
	Current_drive->m_disk_image = new disk_image();
	Current_drive->m_disk_image->init();
	Current_drive->m_disk_image->load_image(disk_image_filename);
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
				auto new_track = std::min(Current_drive->m_disk_image->m_num_tracks - 1, Current_drive->m_half_track_count >> 1);
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
		// load data latch
		break;

	case 0xe:
		Current_drive->m_write_mode = false;
		break;

	case 0xf:
		Current_drive->m_write_mode = true;
		break;
	}

	if (!(addr & 0x1)) {
		return Current_drive->m_data_buffer;
	} else {
		return 0;
	}

}

void write_handler(uint16_t addr, uint8_t val)
{
}

// initialize the disk system
void disk_init(memory &mem)
{
	mem.register_slot_handler(6, read_handler, write_handler);
	drive_1.init();
	drive_2.init();
	Current_drive = &drive_1;
}