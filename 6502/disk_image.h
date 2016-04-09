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
	// constants that will be useful
	static const uint32_t m_total_tracks = 35;
	static const uint32_t m_total_sectors = 16;
	static const uint32_t m_sector_bytes = 256;

	uint8_t*         m_raw_buffer;
	size_t           m_buffer_size;
	uint8_t          m_volume_num;
	std::string      m_filename;
	static uint8_t*  m_work_buffer;

	void initialize_image();
	void nibbilize_track(const int track, uint8_t *buffer);
	void decode_data(uint8_t sector, uint8_t *work_ptr);

public:
	disk_image::disk_image() {}
	disk_image::~disk_image();
	void init();
	bool load_image(const char *filename);
	bool read_track(const int track, uint8_t* buffer);

	image_type   m_image_type;
	uint8_t      m_num_tracks;
};