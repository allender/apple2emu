//
// file to contain information about the disk image that was loaded
//

#pragma once

class disk_image
{

	// defines for different types of disk images we might support
	enum class image_type : uint8_t {
		DSK_IMAGE
	};

private:
	static const uint8_t m_diskbyte_lookup[0x40];
	static const uint8_t m_sector_map[16];

	uint8_t*         m_raw_buffer;
	size_t           m_buffer_size;
	uint8_t          m_volume_num;
	std::string      m_filename;
	static uint8_t*  m_work_buffer;

	void initialize_image();
	uint32_t nibbilize_track(const int track, uint8_t *buffer);

public:
	// constants that will be useful
	static const uint32_t m_total_tracks = 35;
	static const uint32_t m_total_sectors = 16;
	static const uint32_t m_sector_bytes = 256;

	static const uint32_t m_gap1_num_bytes = 48;
	static const uint32_t m_gap2_num_bytes = 6;
	static const uint32_t m_gap3_num_bytes = 27;

	static const uint32_t m_nibbilized_size = 10000;

	disk_image::disk_image() {}
	disk_image::~disk_image();
	void init();
	bool load_image(const char *filename);
	uint32_t read_track(const int track, uint8_t* buffer);

	image_type   m_image_type;
	uint8_t      m_num_tracks;
};