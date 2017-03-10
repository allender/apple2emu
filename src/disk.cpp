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
// source file for disk operations/

#include <algorithm>
#include <iomanip>
#include <SDL_log.h>
#include "apple2emu.h"
#include "disk.h"
#include "memory.h"

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

class disk_drive {
private:
	uint8_t*     m_track_data;       // data read off of the disk put into this buffer
	uint32_t     m_track_size;       // size of the sector data

public:
	disk_image* m_disk_image;       // holds information about the disk image
	bool        m_motor_on;
	bool        m_write_mode;
	bool        m_track_dirty;
	uint8_t     m_phase_status;
	uint8_t     m_half_track_count;
	uint8_t     m_current_track;
	uint8_t     m_data_register;    // data register from controller which holds bytes to/from disk
	uint32_t    m_current_byte;

public:
	disk_drive() :m_disk_image(nullptr) {}
	void init(bool warm_init);
	void readwrite();
	void set_new_track(uint8_t new_track);
	bool insert_disk(const char *filename);
	void eject_disk();
	uint8_t get_num_tracks();
	const char *get_mounted_filename();
};


// I NEED TO FIGURE OUT WHAT THIS DEFINE WORKS
#define NIBBLES_PER_TRACK 0x1A00

static const int Max_drives = 2;

static disk_drive Disk_drives[Max_drives];
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
		m_track_data = new uint8_t[10000];
		if (m_track_data == nullptr) {
			return;
		}
	}

	// read the data out of the disk image into the track image
	if (m_track_size == 0 && m_disk_image != nullptr) {
		SDL_LogVerbose(LOG_CATEGORY_DISK, "track $%02x  read\n", Current_drive->m_current_track);
		m_track_size = m_disk_image->read_track(Current_drive->m_current_track, m_track_data);
		m_track_dirty = false;
		m_current_byte = 0;
	}

	if (m_write_mode == false) {
		// this is read mode
		m_data_register = m_track_data[m_current_byte];
		SDL_LogVerbose(LOG_CATEGORY_DISK, "Read: %04x %02x\n", m_current_byte, m_data_register);
	}
	else {
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
	eject_disk();

	m_disk_image = disk_image::load_image(filename);
	//m_disk_image->init();
	return true;
}

void disk_drive::eject_disk()
{
	if (m_disk_image != nullptr) {
		delete m_disk_image;
		m_disk_image = nullptr;
	}

	init(true);
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
	Disk_drives[slot - 1].insert_disk(disk_image_filename);
	return true;
}

void disk_eject(const uint32_t slot)
{
	Disk_drives[slot - 1].eject_disk();
}

uint8_t drive_handler(uint16_t addr, uint8_t val, bool write)
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
			}
			else {
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
				Current_drive->m_half_track_count = static_cast<uint8_t>(std::max(0, std::min(79, Current_drive->m_half_track_count + dir)));
				uint8_t new_track = static_cast<uint8_t>(std::min(Current_drive->get_num_tracks() - 1, Current_drive->m_half_track_count >> 1));
				if (new_track != Current_drive->m_current_track) {
					Current_drive->set_new_track(new_track);
				}
			}
			SDL_LogVerbose(LOG_CATEGORY_DISK, "phases: %1x%1x%1x%1x track: %02x dir: %1x addr: %02x\n",
				((Current_drive->m_phase_status >> 3) & 0x1),
				((Current_drive->m_phase_status >> 2) & 0x1),
				((Current_drive->m_phase_status >> 1) & 0x1),
				(Current_drive->m_phase_status & 0x1),
				(int)Current_drive->m_half_track_count,
				dir,
				(addr & 0xff));
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
			Current_drive = &Disk_drives[0];
		}
		else {
			Current_drive = &Disk_drives[1];
		}
		SDL_LogVerbose(LOG_CATEGORY_DISK, "Select drive: %1d\n", (addr & 0xf) - 0x9);
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
		}
		else {
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

	if (write == true) {
		Current_drive->m_data_register = val;
	}

	if (!(addr & 0x1)) {
		return Current_drive->m_data_register;
	}

	return 0;

}

// initialize the disk system
void disk_init()
{
	memory_register_slot_handler(6, drive_handler);
	for (int i = 0; i < Max_drives; i++) {
		Disk_drives[i].init(false);
	}
	Current_drive = &Disk_drives[0];
}

void disk_shutdown()
{
	for (int i = 0; i < Max_drives; i++) {
		if (Disk_drives[i].m_disk_image != nullptr) {
			delete Disk_drives[i].m_disk_image;
		}
	}
}

// return the filename of the mounted disk in the given slot
const char *disk_get_mounted_filename(const uint32_t slot)
{
	return Disk_drives[slot - 1].get_mounted_filename();
}

bool disk_is_on(const uint32_t slot)
{
	return Disk_drives[slot - 1].m_motor_on;
}

bool disk_get_track_and_sector(uint32_t slot, uint32_t &track, uint32_t &sector)
{
	track = Disk_drives[slot - 1].m_current_track;
	sector = Disk_drives[slot - 1].m_current_byte / 256;
	return true;
}
