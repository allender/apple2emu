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

disk_drive drive_1;

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
	}
	else {
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
	drive_1.m_disk_image = new disk_image();
	drive_1.m_disk_image->init();
	drive_1.m_disk_image->load_image(disk_image_filename);
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
						drive_1.set_new_track(new_track);
					}
				}
#if defined(LOG_DISK)
				LOG(INFO) << "phases: " << std::setw(1) << ((drive_1.m_phase_status >> 3) & 0x1)
												<< std::setw(1) << ((drive_1.m_phase_status >> 2) & 0x1)
												<< std::setw(1) << ((drive_1.m_phase_status >> 1) & 0x1) 
												<< std::setw(1) << (drive_1.m_phase_status & 0x1)
							 << " track: " << std::setw(2) << (int)drive_1.m_half_track_count
							 << " dir: " << std::setw(1) << dir
							 << " addr: " << std::setbase(16) << (addr & 0xff);
#endif
				}
			}
		break;

	case 0x8:
	case 0x9:
		// turn the motor on or off
		drive_1.m_motor_on = (addr & 1);
		break;

	case 0xa:
	case 0xb:
		// pick a specific drive
		//LOG(INFO) << "Select drive: " << ((addr & 0xf) - 0x9);
		break;

	case 0xc:
		drive_1.readwrite();
		break;
		
	case 0xd:
		// load data latch
		break;

	case 0xe:
		drive_1.m_write_mode = false;
		break;

	case 0xf:
		drive_1.m_write_mode = true;
		break;
	}

	if (!(addr & 0x1)) {
		return drive_1.m_data_buffer;
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
}