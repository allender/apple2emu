#if !defined(MEMORY_H)
#define MEMORY_H

#include <stdint.h>
//#include "video.h"

#define MAX_SLOTS 7

typedef std::function<uint8_t(uint16_t)> slot_io_read_function;
typedef std::function<void(uint16_t, uint8_t)> slot_io_write_function;

#define MAX_TEXT_LINES   24
#define MAX_TEXT_COLUMNS 40

//
// basic memory class.
class memory {

	struct slot_handlers {
		slot_io_read_function    m_read_handler;
		slot_io_write_function   m_write_handler;
	};

public:
	memory() : m_size(0), m_memory(nullptr) { }
	memory(int size, void *memory);

	bool load_memory(const char *filename, uint16_t location);
	bool load_memory(uint8_t *buffer, uint16_t size, uint16_t location);

	// handlers for various i/o
	void register_c000_handler(const uint8_t addr, slot_io_read_function read_function, slot_io_write_function write_function);
	void register_slot_handler(const uint8_t slot, slot_io_read_function read_function, slot_io_write_function write_function);

public:
	uint8_t  operator[] (const uint16_t index);
	void     write(const uint16_t addr, uint8_t val);
	uint8_t  get_screen_char_at(uint32_t row, uint32_t col);
	void     set_page(bool primary);

private:
	uint8_t   *m_memory;  // memory being represented
	int        m_size;    // size of the memory

	uint16_t       m_primary_screen_map[MAX_TEXT_LINES];
	uint16_t       m_secondary_screen_map[MAX_TEXT_LINES];
	uint16_t*      m_screen_map;
	slot_handlers  m_c000_handlers[256];
};


#endif  // MEMORY_H