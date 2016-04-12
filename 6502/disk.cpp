//
// source file for disk operations/

#include <algorithm>
#include <iomanip>
#include "disk.h"
#include "memory.h"

#pragma warning(disable:4996)   // disable the deprecated warnings for fopen

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif


// I NEED TO FIGURE OUT WHAT THIS DEFINE WORKS
#define NIBBLES_PER_TRACK 0x1A00

disk_drive drive_1;

void disk_drive::init()
{
	m_current_track = 0;
	m_current_sector = 0;
	m_half_track_count = 0;
	m_phase_status = 0;
	m_motor_on = false;
	m_sector_data = nullptr;
}

void disk_drive::read()
{
	if (m_sector_data == nullptr) {
		m_sector_data = new uint8_t[disk_image::m_nibbilized_size];
		if (m_sector_data == nullptr) {
			return;
		}
	}

	// read the data out of the disk image into the track image
	bool data_read = m_disk_image->read_sector(drive_1.m_current_track, drive_1.m_current_sector, m_sector_data);
}

// inserts a disk image into the given slot
bool disk_insert(const char *disk_image_filename, const uint32_t slot)
{
	drive_1.m_disk_image = new disk_image();
	drive_1.m_disk_image->init();
	drive_1.m_disk_image->load_image(disk_image_filename);
	return true;
}

static uint8_t ret = 0;

uint8_t& read_handler(uint16_t addr)
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
					drive_1.m_phase_status |= (1<<phase);
				} else {
					drive_1.m_phase_status &= ~(1<<phase);
				}

				// step in (increment) to new track.  phase 1, then phase 2
				// activated from even numbered track.  Phase 3 then phase 0
				// activated from odd numbered track.
				//
				// step out (decrement) to new track.  Phase 3, then phase
				// 2 from even numbered track.  Phase 1, then phase 0
				// from odd numbered track.
				int dir = 0;
				if (drive_1.m_phase_status & (1 << ((drive_1.m_half_track_count + 1) & 3))) {
					dir = 1;
				}
				if (drive_1.m_phase_status & (1 << ((drive_1.m_half_track_count + 3) & 3))) {
					dir = -1;
				}

				if (dir != 0) {
					drive_1.m_half_track_count = std::max(0, std::min(79, drive_1.m_half_track_count + dir));
					auto new_track = std::min(drive_1.m_disk_image->m_num_tracks - 1, drive_1.m_half_track_count >> 1);
					if (new_track != drive_1.m_current_track) {
						drive_1.m_current_track = new_track;
						drive_1.m_current_sector = 0;
					}
				}

				LOG(INFO) << "phases: " << std::setw(1) << ((drive_1.m_phase_status >> 3) & 0x1)
												<< std::setw(1) << ((drive_1.m_phase_status >> 2) & 0x1)
												<< std::setw(1) << ((drive_1.m_phase_status >> 1) & 0x1) 
												<< std::setw(1) << (drive_1.m_phase_status & 0x1)
							 << " track: " << std::setw(2) << (int)drive_1.m_half_track_count
							 << " addr: " << std::setbase(16) << (addr & 0xff);
				
				}
			}
		break;

	case 0x8:
	case 0x9:
		// turn the motor on or off
		drive_1.m_motor_on = (addr & 1);
		LOG(INFO) << "Motor on: " << (addr & 1);
		break;

	case 0xa:
	case 0xb:
		// pick a specific drive
		LOG(INFO) << "Select drive: " << ((addr & 0xf) - 0x9);
		break;

	case 0xc:
		LOG(INFO) << "read";
		drive_1.read();
		break;
		
	case 0xd:
		// load data latch
		break;

	case 0xe:
		drive_1.m_write_mode = false;
		LOG(INFO) << "set read mode";
		break;

	case 0xf:
		drive_1.m_write_mode = true;
		LOG(INFO) << "set write mode";
		break;
	}

	return ret;

}

void write_handler(uint16_t addr, uint8_t val)
{
}

// initialize the disk system
void disk_init(memory &mem)
{
	mem.register_slot_handler(6, read_handler, write_handler);

}