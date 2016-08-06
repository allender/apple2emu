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

#include <stdio.h>
#include <iomanip>
#include <cassert>
#include "memory.h"
#include "video.h"
#include "curses.h"
#include "debugger/debugger.h"
#include "6502/keyboard.h"

#define MEMORY_SIZE (64 * 1024 * 1024)
#define MAX_SLOTS 7

// defines for ram expansion card usage
#define RAM_CARD_READ           (1 << 0)
#define RAM_CARD_BANK2          (1 << 1)
#define RAM_CARD_WRITE_PROTECT  (1 << 2)

// memory buffers.  First buffer is for main memory.  Extended buffer
// is for additional memory (language cards, etc)
static uint8_t *Memory_buffer = nullptr;
static uint8_t *Memory_extended_buffer = nullptr;

static slot_handlers  m_c000_handlers[256];
static uint8_t last_key = 0;
static uint8_t Memory_card_state;

// class to handle memory paging.  Simple wrapper class
// to hold the pointer and whether or not the page is
// write protected
class memory_page {
	
private:
	bool m_write_protected;
	uint8_t *m_ptr;

public:
	memory_page() {};
	void init(uint8_t *ptr, bool write_protected) { m_ptr = ptr; m_write_protected = write_protected; }
	bool write_protected() { return m_write_protected; }
	void set_write_protected(bool write_protected) { m_write_protected = write_protected; }
	uint8_t * get_ptr() { return m_ptr; }
};

memory_page Memory_pages[256];        // there are 256 pages of 256 bytes each page for total of 64k
memory_page Memory_bank1_pages[256];  // includes the same lower 48K plus bank1 and extended RAM
memory_page Memory_bank2_pages[256];  // includes the same lower 48K plus bank2 and extended RAM
memory_page *Memory_current_page_set; // current bank being used

static uint8_t keyboard_read_handler(uint16_t addr)
{
	uint8_t temp_key = keyboard_get_key();
	if (temp_key > 0) {
		last_key = temp_key | 0x80;
	}

	return(last_key);
}

static uint8_t keyboard_clear_read_handler(uint16_t addr)
{
	last_key &= 0x7F;
	return last_key;
}

static void keyboard_clear_write_handler(uint16_t addr, uint8_t val)
{
	keyboard_clear_read_handler(addr);
}

static bool memory_load_from_filename(const char *filename, uint16_t location)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0, SEEK_END);
	uint32_t buffer_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// allocate the memory buffer
	fread(&Memory_buffer[location], 1, buffer_size, fp);
	fclose(fp);
	return true;
}

// loads up appropriate rom images which is based on the
// type of machine we are trying to initialize
static void memory_load_rom_images()
{
	// for now are are al just apple][ +
	memory_load_from_filename("roms/apple2_plus.rom", 0xd000);
	memory_load_from_filename("roms/disk2.rom", 0xc600);
}

// initialize the memory buffer with "random" pattern.  I am
// following the discussion here:
//
// https://github.com/AppleWin/AppleWin/issues/206
// https://github.com/AppleWin/AppleWin/issues/225
// 
// as it covers the issue well.  That and I don't have my apple ][
// anymore on which to test
static void memory_initialize()
{
	memset(Memory_buffer, 0, 0xc000);
	for (auto i = 0; i < 0xc000; i += 4) {
		Memory_buffer[i] = 0xff;
		Memory_buffer[i + 1] = 0xff;

		// same with extended buffer
		Memory_extended_buffer[i] = 0xff;
		Memory_extended_buffer[i + 1] = 0xff;
	}
}

static uint8_t memory_expansion_read_handler(uint16_t addr)
{
	static uint8_t last_access = 0;

	addr = addr & 0xff;
	switch (addr) {

	// cases for bank 2
	case 0x80:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		break;
	case 0x81:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		} else {
			last_access = 1;
		}
		break;
	case 0x82:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		break;
	case 0x83:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		} else {
			last_access = 1;
		}

	// cases for bank 1
	case 0x88:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		break;
	case 0x89:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		} else {
			last_access = 1;
		}
		break;
	case 0x8a:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		break;
	case 0x8b:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		} else {
			last_access = 1;
		}
	}

	// set write protect on ramcard pages if necessary
	for (auto i = 0xd0; i < 0x100; i++) {
		if (Memory_card_state & RAM_CARD_BANK2) {
			Memory_bank2_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT?true:false);
		} else {
			Memory_bank1_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT?true:false);
		}
	}

	// set the current set of pages being used
	if (Memory_card_state & RAM_CARD_READ) {
		if (Memory_card_state & RAM_CARD_BANK2) {
			Memory_current_page_set = Memory_bank2_pages;
		} else {
			Memory_current_page_set = Memory_bank1_pages;
		}
	} else {
		Memory_current_page_set = Memory_pages;
	}

	return 0;
}

static void memory_expansion_write_handler(uint16_t addr, uint8_t val)
{
	memory_expansion_read_handler(addr);
}

uint8_t memory_read(const uint16_t addr)
{
	uint8_t page = (addr / 256);

	// look for memory mapped I/O locations
	if ((addr & 0xff00) == 0xc000) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_read_handler != nullptr ) {
			return m_c000_handlers[mapped_addr].m_read_handler(addr);
		} else {
			LOG(INFO) << "Reading from $" << std::setbase(16) << addr << " and there is no read handler.";
		}
	}
	return *(Memory_current_page_set[page].get_ptr() + (addr & 0xff));
}

// function to write value to memory.  Trapped here in order to
// faciliate memory mapped I/O and other similar things
void memory_write(const uint16_t addr, uint8_t val)
{
	uint8_t page = (addr / 256);

	if (Memory_current_page_set[page].write_protected()) {
		return;
	}

	if ((addr & 0xff00) == 0xc000) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_write_handler != nullptr ) {
			m_c000_handlers[mapped_addr].m_write_handler(addr, val);
			return;
		} else {
			LOG(INFO) << "Writing $" << std::setbase(16) << std::setw(2) << val << " to $" << std::setbase(16) << addr << " and there is no write handler";
		}
	}

	*(Memory_current_page_set[page].get_ptr() + (addr & 0xff)) = val;
}

void memory_register_c000_handler(uint8_t addr, slot_io_read_function read_function, slot_io_write_function write_function)
{
	m_c000_handlers[addr].m_read_handler = read_function;
	m_c000_handlers[addr].m_write_handler = write_function;
}

// register handlers for the I/O slots
void memory_register_slot_handler(const uint8_t slot, slot_io_read_function read_function, slot_io_write_function write_function )
{
	assert((slot >= 0) && (slot <= MAX_SLOTS));
	uint8_t addr = 0x80 + (slot << 4);
	
	// add handlers for the slot.  There are 16 addresses per slot so set them all to the same 
	// handler as we will deal with all slot operations in the same function
	for (auto i = 0; i <= 0xf; i++) {
		m_c000_handlers[addr+i].m_read_handler = read_function;
		m_c000_handlers[addr+i].m_write_handler = write_function;
	}
}

bool memory_load_buffer(uint8_t *buffer, uint16_t size, uint16_t location)
{
   memcpy(&(Memory_buffer[location]), buffer, size);
   return true;
}

// initliaze the memory subsystem
void memory_init()
{
	// init might get called on machine reset - so do some things only once
	if (Memory_buffer == nullptr) {
		Memory_buffer = new uint8_t[MEMORY_SIZE];
		memset(Memory_buffer, 0, MEMORY_SIZE);

		// load rom images based on the type of machine we are starting
		memory_load_rom_images();
	}

	// same for the extended memory buffer.  We'll just allocate the
	// entire 64k because really, why not
	if (Memory_extended_buffer == nullptr) {
		Memory_extended_buffer = new uint8_t[MEMORY_SIZE];
		memset(Memory_extended_buffer, 0, MEMORY_SIZE);
	}

   // initialize memory with "random" pattern.  there was long discussion
   // in applewin github issues tracker related to what to do about
   // memory initialization.  https://github.com/AppleWin/AppleWin/issues/206
   memory_initialize();

	// set up pointers for memory paging.  pointer to each 256 byte page.  Set
	// up write protect for ROM area
	for (auto i = 0; i < 0xd0; i++) {
		Memory_pages[i].init(&Memory_buffer[i * 256], false);
		Memory_bank1_pages[i].init(&Memory_buffer[i * 256], false);
		Memory_bank2_pages[i].init(&Memory_buffer[i * 256], false);
	}
	// this is the ROM area when now using a memory bank
	for (auto i = 0xd0; i < 0x100; i++) {
		Memory_pages[i].init(&Memory_buffer[i * 256], true);
		Memory_bank1_pages[i].init(&Memory_buffer[i * 256], true);
		Memory_bank2_pages[i].init(&Memory_buffer[i * 256], true);
	}

	// set up pointers for ramcard pages
	for (auto i = 0xd0; i < 0xe0; i++) {
		uint32_t addr = (i - 0xd0) * 0x100;
		Memory_bank1_pages[i].init(&Memory_extended_buffer[addr], true);
		Memory_bank2_pages[i].init(&Memory_extended_buffer[0x1000 + addr], true);
	}
	// final 12k, the page pointers are the same.  Offset 8k into extended memory
	// buffer because we have 2 4k pages before that
	for (auto i = 0xe0; i < 0x100; i++) {
		uint32_t addr = (i - 0xe0) * 0x100;
		Memory_bank1_pages[i].init(&Memory_extended_buffer[0x2000 + addr], true);
		Memory_bank2_pages[i].init(&Memory_extended_buffer[0x2000 + addr], true);
	}

	Memory_card_state = 0;
	Memory_card_state = RAM_CARD_BANK2;
	Memory_current_page_set = Memory_pages;

	for (auto i = 0; i < 256; i++) {
		m_c000_handlers[i].m_read_handler = nullptr;
		m_c000_handlers[i].m_write_handler = nullptr;
	}

	// register handlers for keyboard.   There is only a read handler here
	// since we need to read memory to get a key back from the system
	for (auto i = 0; i < 0xf; i++ ) {
		memory_register_c000_handler(0x00, keyboard_read_handler, nullptr);
	}

	// clear keyboard strobe -- this can be a read or write handler
	memory_register_c000_handler(0x10, keyboard_clear_read_handler, keyboard_clear_write_handler);

	// register a handler for memory expansion card in slot 0
	memory_register_slot_handler(0, memory_expansion_read_handler, memory_expansion_write_handler);
}

void memory_shutdown()
{
   if (Memory_buffer != nullptr) {
      delete [] Memory_buffer;
   }
}

