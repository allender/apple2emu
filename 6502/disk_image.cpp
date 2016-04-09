//
// implementation for disk image loading/saving
//

#include "disk_image.h"

#pragma warning(disable:4996)   // disable the deprecated warnings for fopen

uint8_t* disk_image::m_work_buffer = nullptr;

#define CODE44(buf, val) { *buf++ = (((val>>1) & 0x55) | 0xaa); *buf++ = ((val & 0x55) | 0xaa); }

disk_image::~disk_image()
{
	if (m_raw_buffer != nullptr) {
		delete [] m_raw_buffer;
	}
}

void disk_image::initialize_image()
{
	// check the size of the image to see if we have dsk image
	if (m_buffer_size == 143360) {
		m_image_type = image_type::DSK_IMAGE;
		m_num_tracks = (uint8_t)(m_buffer_size / (m_total_sectors * m_sector_bytes));
	}
 }


void disk_image::init()
{
	m_raw_buffer = nullptr;
	m_work_buffer = new uint8_t[m_total_sectors * m_sector_bytes];
}

bool disk_image::load_image(const char *filename)
{
	// for now, just load the disk image into a buffer and we will use
	// that to load

	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0, SEEK_END);
	m_buffer_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// allocate the whole buffer - in modern systems we
	// can just load these things up and not really worry
	// about memory usage here
	m_raw_buffer = (uint8_t *)new uint8_t[m_buffer_size];
	fread(m_raw_buffer, 1, m_buffer_size, fp);
	fclose(fp);

	// get all the information needed about this disk image
	initialize_image();
	m_filename = filename;
	m_volume_num = 254;
	return true; 
}

void disk_image::nibbilize_track(const int track, uint8_t *buffer)
{
	// get a pointer to the beginning of the track information
	uint8_t *track_ptr = &m_raw_buffer[track * (m_total_sectors * m_sector_bytes)];

	uint8_t *work_ptr = buffer;  // working pointer into the work buffer

	// write out the self-sync bytes.  I'm pretty sure that we can put
	// in range between X and Y self sync bytes
	for (auto i = 0; i < 32; i++) {
		*work_ptr++ = 0xff;
	}

	for (auto i = 0; i < m_total_sectors; i++) {
		// read in the sector, which consists of 
		// Address Field
		//    Prologue    D5 AA 96
		//    Volume      4 and 4 Volume 
		//    Track       4 and 4 of track number
		//    Sector      4 and 4 of secgtor
		//    Checksum    volume ^ track ^ sector
		//    Eplilogue   DE AA EB
		// Gap 2
		// Data Field
		//    Prologue     D4 AA AD
		//    343 bytes of data
		//    checksum
		//    Epilogue     DE AA EB

		// Address
		*work_ptr++ = 0xd5;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0x96;
		CODE44(work_ptr, m_volume_num);
		CODE44(work_ptr, track);
		CODE44(work_ptr, i);
		CODE44(work_ptr, (uint8_t)m_volume_num ^ (uint8_t)track ^ (uint8_t)i);
		*work_ptr++ = 0xde;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xeb;

		// gap 2
		for ( auto j = 0 ; j < 8; j++ ) {
			*work_ptr++ = 0xff;
		}

		// data field
		*work_ptr++ = 0xd5;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xad;
		
		// convert 256 bytes into 342 bytes + checksum
		uint8_t offset = 0x56 + 0x56;   // start at offset 0xac since bytes start there
		while (offset < 0xff) {
			uint8_t val = ((track_ptr[offset] & 0x1) << 1) | ((track_ptr[offset] & 0x2) >> 1);
			val <<= 2;
			val = val | (((track_ptr[offset - 0x56] & 0x1) << 1) | ((track_ptr[offset - 0x56] & 0x2) >> 1));
			val <<= 2;
			val = val | (((track_ptr[offset - 0x56 - 0x56] & 0x1) << 1) | ((track_ptr[offset - 0x56 - 0x56] & 0x2) >> 1));
			val <<= 2;
			*work_ptr++ = val;
			offset++;
		}


		*work_ptr++ = 0xde;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xeb;

	}

}

// read the track data into the supplied buffer
bool disk_image::read_track(const int track, uint8_t *buffer)
{
	if (m_work_buffer == nullptr) {
		return false;
	}

	// with the data in the work buffer, we need to nibbilize the data
	nibbilize_track(track, buffer);

	return true;
}