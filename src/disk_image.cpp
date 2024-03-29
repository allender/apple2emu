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
// implementation for disk image loading/saving
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "apple2emu_defs.h"
#include "disk_image.h"

const uint8_t disk_image::m_sector_map[static_cast<uint8_t>(format_type::NUM_FORMATS)][16] =
{
   { 0x00,0x07,0x0E,0x06,0x0D,0x05,0x0C,0x04,0x0B,0x03,0x0A,0x02,0x09,0x01,0x08,0x0F },  // DOS format
   { 0x00,0x08,0x01,0x09,0x02,0x0A,0x03,0x0B,0x04,0x0C,0x05,0x0D,0x06,0x0E,0x07,0x0F },  // prodos format
};

// prodos block to physical sector
const uint8_t disk_image::m_prodos_block_map[][2] =
{
	{ 0x0, 0x2 },
	{ 0x4, 0x6 },
	{ 0x8, 0xa },
	{ 0xc, 0xe },
	{ 0x1, 0x3 },
	{ 0x5, 0x7 },
	{ 0x9, 0xb },
	{ 0xd, 0xf },
};

// destructor for a diskimage.  Make sure the
// image is saved before deleting it
disk_image::~disk_image()
{
	// save the image if we have written to it
	save_image();
	if (m_raw_buffer != nullptr) {
		delete[] m_raw_buffer;
	}
}

const char *disk_image::get_filename()
{
	return m_filename.c_str();
}


void disk_image::init()
{
	m_raw_buffer = nullptr;
	m_filename.clear();
}

// load an image from disk.  Read the entirety
// of the image into local memory.  Makes
// reading and writing super fast since we do it
// straight to memory
disk_image *disk_image::load_image(const char *filename)
{
	// load disk into buffer
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return nullptr;
	}

	fseek(fp, 0, SEEK_END);
	uint32_t buffer_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// allocate the whole buffer - in modern systems we
	// can just load these things up and not really worry
	// about memory usage here
	uint8_t *raw_buffer = (uint8_t *)new uint8_t[buffer_size];
	fread(raw_buffer, 1, buffer_size, fp);
	fclose(fp);

	// determine read-only access for the file
	bool read_only = false;
	if (access(filename, 2)) {
		read_only = true;
	}

	// some logic to determine which kind of disk we have
	const char *ext = strrchr(filename, '.');
	disk_image *new_image = nullptr;
	if (ext != nullptr) {
		if ((!stricmp(ext, ".dsk") || !stricmp(ext, ".do")) && buffer_size == m_dsk_image_size) {
			// disk image format
			new_image = new dsk_image();
		}

		else if  (!stricmp(ext, ".nib") && buffer_size == m_nib_image_size) {
			new_image = new nib_image();
		}

		else if  (!stricmp(ext, ".po") && buffer_size == m_dsk_image_size) {
			new_image = new po_image();
		}
	}

	if (new_image != nullptr) {
        new_image->m_read_only = read_only;
		new_image->m_raw_buffer = raw_buffer;
		new_image->m_buffer_size = buffer_size;
		new_image->m_filename = filename;
		new_image->m_volume_num = 254;
		new_image->m_image_dirty = false;
		new_image->initialize_image();
	}


	return new_image;
}

// save a disk image (if needed).  This will write out the entire disk
// image which is stored in memory
bool disk_image::save_image()
{
	if (m_image_dirty == true) {
		FILE *fp = fopen(m_filename.c_str(), "wb");
		if (fp == nullptr) {
			return false;
		}
		size_t num_written = fwrite(m_raw_buffer, 1, m_buffer_size, fp);
		fclose(fp);
		if (num_written != m_buffer_size) {
			printf("Didn't write out full image!\n");
		}
		m_image_dirty = false;
	}
	return true;
}

// unload a disk image.  Called when a new disk is inserted.  This call will
// clean up the image (saving if necessary)
bool disk_image::unload_image()
{
	// save the image if we have written to it
	save_image();
	if (m_raw_buffer != nullptr) {
		delete[] m_raw_buffer;
	}
	m_filename.clear();

	return true;
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
//  Code for DSK Images
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------

void dsk_image::initialize_image()
{
    m_format = format_type::DOS_FORMAT;
    m_num_tracks = m_total_tracks;
}

#define CODE44(buf, val) { *buf++ = ((((val)>>1) & 0x55) | 0xaa); *buf++ = (((val) & 0x55) | 0xaa); }

// translate table used during nibbilize process to go
// from 6-bit bytes to 8 bit disk bytes
const uint8_t disk_image::m_write_translate_table[64] =
{
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

// this table is used during de-nibbilize to go
// from 8 bit disk byte to 6 bit bytes.  The table
// is _really_ 64 entries large because we only
// have a one to one mapping from disk bytes
// to 6 bit bytes.  But doing that mapping would
// require std::map, and while that's fine, it's
// easier to just create this table once and leave it
// as a static variable.  While we are truly only
// using 64 of these entires (one to one mapping of
// disk bytes to 6 bit bytes), then table needs to
// be large because we have holse in the read translate
// table.  All disk bytes have high bit set so there
// are 128 _potential_ entries, although only 64 are
// used
const uint8_t disk_image::m_read_translate_table[128] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,
	0x00,0x00,0x08,0x0c,0x00,0x10,0x14,0x18,
	0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x20,
	0x00,0x00,0x00,0x24,0x28,0x2c,0x30,0x34,
	0x00,0x00,0x38,0x3c,0x40,0x44,0x48,0x4c,
	0x00,0x50,0x54,0x58,0x5c,0x60,0x64,0x68,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x6c,0x00,0x70,0x74,0x78,
	0x00,0x00,0x00,0x7c,0x00,0x00,0x80,0x84,
	0x00,0x88,0x8c,0x90,0x94,0x98,0x9c,0xa0,
	0x00,0x00,0x00,0x00,0x00,0xa4,0xa8,0xac,
	0x00,0xb0,0xb4,0xb8,0xbc,0xc0,0xc4,0xc8,
	0x00,0x00,0xcc,0xd0,0xd4,0xd8,0xdc,0xe0,
	0x00,0xe4,0xe8,0xec,0xf0,0xf4,0xf8,0xfc,
};

// the nuibbilize function.  This code (and the following denibbilize code) can be
// pretty difficult to understsand.  For those playing at home, the following
// reference is invaluable:
//
// http://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Books/Beneath%20Apple%20ProDOS.pdf
//
// In essence, .DSK images contains track/sector data that is _not_
// in the same format that an original apple ][ disk.  The apple ][
// disks store their data in a nibbilized format.  When the nibbilized
// data is read off of disk, the driver will denibbilize the data which
// turns in into the real data used by the computer.  On writing,
// the real data is nibbilized and then stored on disk.
//
// Because the .DSK images store the raw data (i.e. not nibbilized data), we
// must perform the opposite operation on read/write.  Meaning, when we
// read the data out of a .DSK image, we need to nibbilize the data before
// handing it to the driver.  When we write the data, it will be nibbilized
// so we must denibbilize the data before writing it to the .DSK image.
// The following two routines take care of nibbilizing and denibbilizing.
// The code works on a track at a time (i.e. 16 sectors for dos 3.3 format)
// to make things speedier.
uint32_t disk_image::nibbilize_track(const int track, uint8_t *buffer)
{
	// get a pointer to the beginning of the track information
	uint8_t *track_ptr = &m_raw_buffer[track * (m_total_sectors * m_sector_bytes)];
	uint8_t *work_ptr = buffer;  // working pointer into the final data

	// write out the self-sync bytes.  I'm pretty sure that we can put
	// in range between X and Y self sync bytes
	for (uint32_t i = 0; i < m_gap1_num_bytes; i++) {
		*work_ptr++ = 0xff;
	}

	for (uint32_t sector = 0; sector < m_total_sectors; sector++) {
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
		// Gap 3

		// Address
		*work_ptr++ = 0xd5;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0x96;
		CODE44(work_ptr, m_volume_num);
		CODE44(work_ptr, track);
		CODE44(work_ptr, sector);
		CODE44(work_ptr, (uint8_t)m_volume_num ^ (uint8_t)track ^ (uint8_t)sector);
		*work_ptr++ = 0xde;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xeb;

		// gap 2
		for (uint32_t i = 0; i < m_gap2_num_bytes; i++) {
			*work_ptr++ = 0xff;
		}

		// data field
		*work_ptr++ = 0xd5;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xad;

		// convert 256 bytes into 342 bytes + checksum
		{
			uint8_t *nib_data = (uint8_t *)alloca(344);

			// map the sector (0-16) to the logical sector using the sector map.  Done for interleaving
			uint8_t mapped_sector = m_sector_map[static_cast<uint8_t>(m_format)][sector];
			uint8_t *sector_ptr = &track_ptr[mapped_sector * m_sector_bytes];

			memcpy(&nib_data[86], sector_ptr, 256);   // copy the track data into the allocated buffer
			nib_data[342] = 0;                       // this is the last byte and needs to be 0 because of the checksumming process that creates additional byte

			// nibbilize the data
			// do 6/2 encoding
			uint8_t offset = 0x0;
			while (offset < 0x56) {
				uint8_t val = (((sector_ptr[(uint8_t)(offset + 0xac)] & 0x1) << 1) | ((sector_ptr[(uint8_t)(offset + 0xac)] & 0x2) >> 1)) << 6;
				val = val | ((((sector_ptr[(uint8_t)(offset + 0x56)] & 0x1) << 1) | ((sector_ptr[(uint8_t)(offset + 0x56)] & 0x2) >> 1)) << 4);
				val = val | (((sector_ptr[offset] & 0x1) << 1) | ((sector_ptr[offset] & 0x2) >> 1)) << 2;
				nib_data[offset++] = val;
			}

			// not sure why we have to do this
			nib_data[offset - 1] &= 0x3f;
			nib_data[offset - 2] &= 0x3f;

			// checksum the entire buffer
			uint8_t xor_value = 0;
			for (auto i = 0; i <= 343; i++) {
				auto prev_val = nib_data[i];
				nib_data[i] = nib_data[i] ^ xor_value;
				xor_value = prev_val;
			}

			// translate the 6-bit bytes into disk bytes directly to the work buffer
			for (auto i = 0; i <= 342; i++) {
				*work_ptr++ = m_write_translate_table[nib_data[i] >> 2];
			}
		}

		*work_ptr++ = 0xde;
		*work_ptr++ = 0xaa;
		*work_ptr++ = 0xeb;

		// gap 3
		for (uint32_t i = 0; i < m_gap3_num_bytes; i++) {
			*work_ptr++ = 0xff;
		}
	}

	return static_cast<uint32_t>(work_ptr - buffer);  // number of bytes "read"
}

// denibbilize a track.  See the comments above the previous
// function for an explanation.
bool disk_image::denibbilize_track(const int track, uint8_t *buffer)
{
	uint8_t *track_ptr = &m_raw_buffer[track * (m_total_sectors * m_sector_bytes)];
	uint8_t *work_ptr = buffer;  // working pointer into the final data

	for (auto num_sectors = 0; num_sectors < 16; num_sectors++) {

		// skip past gap 1
		while (*work_ptr == 0xff) {
			work_ptr++;
		}

		// this is the address field (d5 aa 96)
		uint8_t prologue[3];
		for (auto i = 0; i < 3; i++) {
			prologue[i] = *work_ptr++;
		}
		if (prologue[0] != 0xd5 || prologue[1] != 0xaa || prologue[2] != 0x96) {
			// prologue doesn't match.  It should  Can we even do anything about this?
			return false;
		}

		// need to get the sector number.  Let's also verify that the track number
		// matches
		work_ptr += 2;  // skip past the volume number
#if defined(_DEBUG)
		uint8_t encoded_track = (*work_ptr & 0x55) << 1 | (*(work_ptr + 1) & 0x55);
		SDL_assert(encoded_track == track);
#endif
		work_ptr += 2;

		uint8_t encoded_sector = (*work_ptr & 0x55) << 1 | (*(work_ptr + 1) & 0x55);
		work_ptr += 2;
		uint8_t mapped_sector = m_sector_map[static_cast<uint8_t>(m_format)][encoded_sector];
		uint8_t *sector_ptr = &track_ptr[mapped_sector * m_sector_bytes];

		work_ptr += 2;  // skip past the checksum
		work_ptr += 3;  // skip past the epilogue

		// skip past gap 2
		while (*work_ptr == 0xff) {
			work_ptr++;
		}
		for (auto i = 0; i < 3; i++) {
			prologue[i] = *work_ptr++;
		}
		if (prologue[0] != 0xd5 || prologue[1] != 0xaa || prologue[2] != 0xad) {
			// prologue doesn't match.  It should.  Can we even do anything about this?
			return false;
		}

		// convert disk bytes back to 6 bit bytes
		// first do the reverse lookup from the read table.  This mapping
		// goes from disk bytes to 6 bit bytes
		uint8_t *nib_data = (uint8_t *)alloca(344);
		for (auto count = 0; count <= 343; count++) {
			nib_data[count] = m_read_translate_table[(*work_ptr++) & 0x7f];
		}

		// skip epilogue
		work_ptr += 5;

		// xor the buffer with itself
		uint8_t xor_value = 0;
		for (auto i = 0; i < 343; i++) {
			nib_data[i] = nib_data[i] ^ xor_value;
			xor_value = nib_data[i];
		}

		// now convert 6 bit bytes to 256 8 bit bytes.  Value is stored in the track itself (presumably for
		// writing soon after this is done).  Start at offset 0x56 into the data as that is there the
		// 6 bit bytes are stored.  These will be combined with the bits in the first 0x56 bytes to
		// form the true 256 bytes for storage
		for (auto byte_num = 0; byte_num < 0x56; byte_num++) {
			sector_ptr[byte_num] = (nib_data[byte_num + 0x56] & 0xfc) | ((nib_data[byte_num] & 0x08) >> 3) | ((nib_data[byte_num] & 0x04) >> 1);
			sector_ptr[byte_num + 0x56] = (nib_data[byte_num + 0x56 + 0x56] & 0xfc) | ((nib_data[byte_num] & 0x20) >> 5) | ((nib_data[byte_num] & 0x10) >> 3);
			if (byte_num + 0xac < 0x100) {
				sector_ptr[byte_num + 0xac] = (nib_data[byte_num + 0xac + 0x56] & 0xfc) | ((nib_data[byte_num] & 0x80) >> 7) | ((nib_data[byte_num] & 0x40) >> 5);
			}
		}
	}

	return true;
}

// read the track data into the supplied buffer
uint32_t dsk_image::read_track(const uint32_t track, uint8_t *buffer) {
	// with the data in the work buffer, we need to nibbilize the data
	return nibbilize_track(track, buffer);
}

bool dsk_image::write_track(const uint32_t track, uint8_t *buffer)
{
	// denybbilze track data stored in buffer to the work buffer
	// and then store that work buffer into the loaded disk image
	m_image_dirty = true;
	return denibbilize_track(track, buffer);
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
//  Code for NIB Images
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------

void nib_image::initialize_image()
{
    m_format = format_type::DOS_FORMAT;
    m_num_tracks = m_total_tracks;
}

// read the track data into the supplied buffer
uint32_t nib_image::read_track(const uint32_t track, uint8_t *buffer) {
	// nib images have their data already in the format that we need
	uint32_t num_bytes = m_total_sectors * m_sector_bytes;
	memcpy(buffer, &m_raw_buffer[track * num_bytes], num_bytes);
	return num_bytes;
}

// NIB images can just get written straight out
bool nib_image::write_track(const uint32_t track, uint8_t *buffer)
{
	uint32_t num_bytes = m_total_sectors * m_sector_bytes;
	uint8_t *track_ptr = &m_raw_buffer[track * num_bytes];
    memcpy(track_ptr, buffer, num_bytes);
	return true;
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
//  Code for PO Images
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------

void po_image::initialize_image()
{
    m_format = format_type::PRODOS_FORMAT;
    m_num_tracks = m_total_tracks;
}

// read the track data into the supplied buffer
uint32_t po_image::read_track(const uint32_t track, uint8_t *buffer) {
	// with the data in the work buffer, we need to nibbilize the data
	return nibbilize_track(track, buffer);
}

bool po_image::write_track(const uint32_t track, uint8_t *buffer)
{
	// denybbilze track data stored in buffer to the work buffer
	// and then store that work buffer into the loaded disk image
	m_image_dirty = true;
	return denibbilize_track(track, buffer);
}

