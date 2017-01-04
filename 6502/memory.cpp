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
#include "apple2emu.h"
#include "memory.h"
#include "video.h"
#include "curses.h"
#include "debugger/debugger.h"
#include "6502/keyboard.h"

#define MAX_SLOTS 7

// defines for memory status (RAM card, 0xc000 usage, etc)
#define RAM_CARD_READ           (1 << 0)
#define RAM_CARD_BANK2          (1 << 1)
#define RAM_CARD_WRITE_PROTECT  (1 << 2)
#define RAM_SLOTCX_ROM          (1 << 3)
#define RAM_SLOTC3_ROM          (1 << 4)

// values for memory sizes
static const int32_t Memory_main_size = (48 * 1024);
static const int32_t Memory_rom_size = (16 * 1024);
static const int32_t Memory_switched_bank_size = (4 * 1024);
static const int32_t Memory_c000_rom_size = (4 * 1024);
static const int32_t Memory_extended_size = (8 * 1024);

static const int32_t Memory_page_size = 256;

// and page bvalues
static const int32_t Memory_num_main_pages = (Memory_main_size / Memory_page_size);
static const int32_t Memory_num_rom_pages = (Memory_rom_size / Memory_page_size);
static const int32_t Memory_num_bank_pages = (Memory_switched_bank_size / Memory_page_size);
static const int32_t Memory_num_c000_pages = (Memory_c000_rom_size/ Memory_page_size);
static const int32_t Memory_num_extended_pages = (Memory_extended_size / Memory_page_size);

// iie roms are 16k in size.  c000 to cfff can have different
// functions depending on iie soft switches
static const int APPLE2E_ROM_SIZE = 0x4000;
static const int C000_ROM_SIZE = 0x1000;

static slot_handlers  m_c000_handlers[256];
static uint8_t last_key = 0;
static uint8_t Memory_card_state;

// class to handle memory paging.  Simple wrapper class  to hold the
// pointer and whether or not the page is write protected.  Memory
// pages will be set up when emulator starts.  Pointers to a full set
// of 64K will be stored and those pointers will point to memory_page(s)
// depending on which soft switches have been set
class memory_page {

private:
	bool m_write_protected;
	uint8_t *m_ptr;

public:
	memory_page() {};

	// init sets up the pointer to the actually memory and initial
	// write protect status
	void init(uint8_t *ptr, bool write_protected) {
		m_ptr = ptr;
		m_write_protected = write_protected;
	}

	// returns write protect status
	bool const write_protected() const {
		return m_write_protected;
	}

	// sets write protect status
	void set_write_protected(bool write_protected) {
		m_write_protected = write_protected;
	}

	// returns the pointer to memory
	uint8_t * get_ptr() const { return m_ptr; }
};

// main definition of memory pages for the emulator.  There is a
// page array for each "type" of memory (i.e. main, rom, extended
// auxilliary, etc).  
memory_page Memory_main_pages[Memory_num_main_pages];         // 192 pages - 48k
memory_page Memory_rom_pages[Memory_num_rom_pages];           // 64 pages - 16k
memory_page Memory_bank1_pages[Memory_num_bank_pages];        // 16 pages - 4k
memory_page Memory_bank2_pages[Memory_num_bank_pages];        // 16 pages - 4k
memory_page Memory_extended_pages[Memory_num_extended_pages]; // 32 pages - 8k

// pointers for read/write pages.  These are the methods by
// which memory reading and writing will be done.  We have
// separate read/write arrays because the apple allows writing
// to RAM banks while allowing reading from ROM area
memory_page *Memory_read_pages[256];
memory_page *Memory_write_pages[256];

// Below are actually memory buffers.  The memory pointers for
// all of the memory_pages(s) will point into these buffers

// main buffer of 48k.
static uint8_t *Memory_buffer = nullptr;

// rom buffer - 16k.  For apple2, c000 - cfff
// isn't loaded from rom image, but are only used
// for soft switches.  For apple2e, the full
// 16k will be loaded
static uint8_t *Memory_rom_buffer = nullptr;

// two buffers for 4k ram (bank1 and bank2)
static uint8_t *Memory_bank1_buffer = nullptr;
static uint8_t *Memory_bank2_buffer = nullptr;

// one 12k buffer for extended ram 
static uint8_t *Memory_extended_buffer = nullptr;

static uint8_t *Memory_default_c000_buffer = nullptr;

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

static bool memory_load_from_filename(const char *filename, uint8_t *dest)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0, SEEK_END);
	uint32_t buffer_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// if we are loading a iie rom, then we need to store off the lower
	// 4k into another buffer for use when certain soft switches
	// are used
	if (buffer_size == APPLE2E_ROM_SIZE) {
		fread(Memory_default_c000_buffer, 1, 0x1000, fp);
		buffer_size -= 0x1000;
	}

	// allocate the memory buffer
	fread(dest, 1, buffer_size, fp);
	fclose(fp);
	return true;
}

// loads up appropriate rom images which is based on the
// type of machine we are trying to initialize
static void memory_load_rom_images()
{
	if (Emulator_type == emulator_type::APPLE2) {
		memory_load_from_filename("roms/apple2.rom", &Memory_rom_buffer[0x1000]);
	}
	else if (Emulator_type == emulator_type::APPLE2_PLUS) {
		memory_load_from_filename("roms/apple2_plus.rom", &Memory_rom_buffer[0x1000]);
	} else if (Emulator_type == emulator_type::APPLE2E) {
		memory_load_from_filename("roms/apple2e.rom", &Memory_rom_buffer[0]);
	}
	memory_load_from_filename("roms/disk2.rom", &Memory_rom_buffer[0x600]);
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
	}

	// same with bank  buffers
	for (auto i = 0; i < Memory_switched_bank_size; i += 4) {
		Memory_bank1_buffer[i] = 0xff;
		Memory_bank1_buffer[i + 1] = 0xff;
		Memory_bank2_buffer[i] = 0xff;
		Memory_bank2_buffer[i + 1] = 0xff;
	}

	// same with extended buffer
	for (auto i = 0; i < Memory_extended_size; i += 4) {
		Memory_extended_buffer[i] = 0xff;
		Memory_extended_buffer[i + 1] = 0xff;
	}
}

// set up the paging table pointers based on the current memory
// setup.  
static void memory_set_paging_tables()
{
	// set up the main 48k.
	for (auto page = 0; page < Memory_num_main_pages; page++) {
		Memory_read_pages[page] = &Memory_main_pages[page];
		Memory_write_pages[page] = &Memory_main_pages[page];
	}

	// set up the rom/extended memory paging.  This will be either bank 1
	// or bank 2 and the extended buffer.  First start with the read pages
	if (Memory_card_state & RAM_CARD_READ) {
		// set up the pages for the first 4k bank
		if (Memory_card_state & RAM_CARD_BANK2) {
			for (auto i = 0; i < Memory_num_bank_pages; i++) {
				Memory_read_pages[0xd0 + i] = &Memory_bank2_pages[i];
			}
		} else {
			for (auto i = 0; i < Memory_num_bank_pages; i++) {
				Memory_read_pages[0xd0 + i] = &Memory_bank1_pages[i];
			}
		}

		// set the paging table pointers to the extended buffer
		for (auto i = 0; i < Memory_num_extended_pages; i++) {
			Memory_read_pages[0xe0 + i] = &Memory_extended_pages[i];
		}
	}
	
	// for rom reading, point to the rom pages.  Have to take into
	// account whether we are running apple ii or iie as we might
	// want to load builtin rom for the 2e
	else {
		if (Emulator_type != emulator_type::APPLE2E) {
			// for apple 2 and 2+, skip the first 16 pages (0xc000 - 0xcfff)
			for (auto i = 0; i < Memory_num_rom_pages; i++ ) {
				Memory_read_pages[0xc0 + i] = &Memory_rom_pages[i];
			}
		}
	}

	// set up the pages for writing
	if (Memory_card_state & RAM_CARD_WRITE_PROTECT) {
		// if the ram card is write protected, then set up pages for writing
		// to rom, which won't really do anything
		if (Emulator_type != emulator_type::APPLE2E) {
			// for apple 2 and 2+, skip the first 16 pages/ (0xc000 - 0xcfff)
			for (auto i = 0; i < Memory_num_rom_pages; i++) {
				Memory_write_pages[0xc0 + i] = &Memory_rom_pages[i];
			}
		}
	}

	// writing is enabled to the extended ram space
	else {
		// set up the pages for the first 4k bank
		if (Memory_card_state & RAM_CARD_BANK2) {
			for (auto i = 0; i < Memory_num_bank_pages; i++) {
				Memory_write_pages[0xd0 + i] = &Memory_bank2_pages[i];
			}
		} else {
			for (auto i = 0; i < Memory_num_bank_pages; i++) {
				Memory_write_pages[0xd0 + i] = &Memory_bank1_pages[i];
			}
		}

		// set the paging table pointers to the extended buffer
		for (auto i = 0; i< Memory_num_extended_pages; i++) {
			Memory_write_pages[0xe0 + i] = &Memory_extended_pages[i];
		}
	}
}

// Read/write handler for the memory expansion card in SLOT 0.  A 16K
// card for the apple ii and ii plus.  The information here was
// mainly obtained from the following document:
//
// http://apple2online.com/web_documents/microsoft_ramcard_-_manual.pdf
//
// also this is the appleiie memory format.  Can be found in the Apple2e
// reference manual (chapter 4)
//
// http://apple2online.com/web_documents/Apple%20IIe%20Technical%20Reference%20Manual%20KB.pdf
//
static uint8_t memory_expansion_read_handler(uint16_t addr)
{
	static uint8_t last_access = 0;

	addr = addr & 0xff;
	switch (addr) {

		// cases for bank 2
	case 0x80:
	case 0x84:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x81:
	case 0x85:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;
	case 0x82:
	case 0x86:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x83:
	case 0x87:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state |= RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}

		// cases for bank 1
	case 0x88:
	case 0x8c:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x89:
	case 0x8d:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;
	case 0x8a:
	case 0x8e:
		Memory_card_state &= ~RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x8b:
	case 0x8f:
		Memory_card_state |= RAM_CARD_READ;
		Memory_card_state &= ~RAM_CARD_BANK2;
		Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_card_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
	}

	// set write protect on ramcard pages if necessary.  Total of 48 pages
	for (auto i = 0; i < Memory_num_bank_pages; i++) {
		if (Memory_card_state & RAM_CARD_BANK2) {
			Memory_bank2_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT ? true : false);
		} else {
			Memory_bank1_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT ? true : false);
		}
	}
	for (auto i = 0; i < Memory_num_extended_pages; i++) {
		Memory_extended_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT ? true : false);
	}

	// set up paging pointers
	memory_set_paging_tables();

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
	if (page == 0xc0) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_read_handler != nullptr) {
			return m_c000_handlers[mapped_addr].m_read_handler(addr);
		}
		else {
			//LOG(INFO) << "Reading from $" << std::setbase(16) << addr << " and there is no read handler.";
		}
	}
	return *(Memory_read_pages[page]->get_ptr() + (addr & 0xff));
}

// function to write value to memory.  Trapped here in order to
// faciliate memory mapped I/O and other similar things
void memory_write(const uint16_t addr, uint8_t val)
{
	uint8_t page = (addr / 256);

	if (page == 0xc0) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_write_handler != nullptr) {
			m_c000_handlers[mapped_addr].m_write_handler(addr, val);
			return;
		}
		else {
			//LOG(INFO) << "Writing $" << std::setbase(16) << std::setw(2) << val << " to $" << std::setbase(16) << addr << " and there is no write handler";
		}
	}

	if (Memory_write_pages[page]->write_protected()) {
		return;
	}

	*(Memory_write_pages[page]->get_ptr() + (addr & 0xff)) = val;
}

void memory_register_c000_handler(uint8_t addr, slot_io_read_function read_function, slot_io_write_function write_function)
{
	m_c000_handlers[addr].m_read_handler = read_function;
	m_c000_handlers[addr].m_write_handler = write_function;
}

// register handlers for the I/O slots
void memory_register_slot_handler(const uint8_t slot, slot_io_read_function read_function, slot_io_write_function write_function)
{
	assert((slot >= 0) && (slot <= MAX_SLOTS));
	uint8_t addr = 0x80 + (slot << 4);

	// add handlers for the slot.  There are 16 addresses per slot so set them all to the same 
	// handler as we will deal with all slot operations in the same function
	for (auto i = 0; i <= 0xf; i++) {
		m_c000_handlers[addr + i].m_read_handler = read_function;
		m_c000_handlers[addr + i].m_write_handler = write_function;
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
	// main memory buffer - 48k
	if (Memory_buffer == nullptr) {
		Memory_buffer = new uint8_t[Memory_main_size];
	}
	memset(Memory_buffer, 0, Memory_main_size);

	// main ROM buffer - 16k
	if (Memory_rom_buffer == nullptr) {
		Memory_rom_buffer = new uint8_t[Memory_rom_size];
	}
	memset(Memory_rom_buffer, 0xff, Memory_rom_size);

	// memory for c000-cfff rom (used in apple2e).  Holds
	// the c000-cfff rom from the rom images which gets
	// used depending on soft switches
	if (Memory_default_c000_buffer == nullptr) {
		Memory_default_c000_buffer = new uint8_t[C000_ROM_SIZE];
	}
	memset(Memory_default_c000_buffer, 0, C000_ROM_SIZE);

	// load rom images based on the type of machine we are starting
	memory_load_rom_images();

	// same for the extended memory buffer.  We'll just allocate the
	// entire 64k because really, why not
	if (Memory_bank1_buffer == nullptr) {
		Memory_bank1_buffer = new uint8_t[Memory_switched_bank_size];
	}
	if (Memory_bank2_buffer == nullptr) {
	  Memory_bank2_buffer = new uint8_t[Memory_switched_bank_size];
	}
	memset(Memory_bank1_buffer, 0, Memory_switched_bank_size);
	memset(Memory_bank2_buffer, 0, Memory_switched_bank_size);

	// This is the 12k buffer that is active for either bank
	// 1 or bank 2
	if (Memory_extended_buffer == nullptr) {
	  Memory_extended_buffer = new uint8_t[Memory_extended_size];
	}

	memset(Memory_extended_buffer, 0, Memory_extended_size);

	// initialize memory with "random" pattern.  there was long discussion
	// in applewin github issues tracker related to what to do about
	// memory initialization.  https://github.com/AppleWin/AppleWin/issues/206
	memory_initialize();

	// main memory area page pointers.  This is not write protected.  There
	// are 
	for (auto i = 0; i < Memory_main_size / 256; i++) {
		Memory_main_pages[i].init(&Memory_buffer[i * 256], false);
	}

	// ROM area.  This is write protected
	for (auto i = 0; i < Memory_rom_size / 256; i++) {
		Memory_rom_pages[i].init(&Memory_rom_buffer[i * 256], true);
	}

	// set up pointers for ramcard pages
	for (auto i = 0; i < Memory_switched_bank_size / 256; i++) {
	  uint32_t addr = i * 256;
	  Memory_bank1_pages[i].init(&Memory_bank1_buffer[addr], false);
	  Memory_bank2_pages[i].init(&Memory_bank2_buffer[addr], false);
	}

	// final 12k, the page pointers are the same.  Offset 8k into extended memory
	// buffer because we have 2 4k pages before that
	for (auto i = 0; i < Memory_extended_size / 256; i++) {
	  uint32_t addr = i * 256;
	  Memory_extended_pages[i].init(&Memory_extended_buffer[addr], false);
	}

	// set up the main memory card state
	Memory_card_state = RAM_CARD_BANK2 | RAM_SLOTCX_ROM;
	if (Emulator_type != emulator_type::APPLE2E) {
		Memory_card_state |= RAM_CARD_WRITE_PROTECT;
	}

	// update paging based on the current memory configuration.  this
	// will set up pointers to the memory pages to point to the
	// correct pages
	memory_set_paging_tables();

	for (auto i = 0; i < Memory_main_size / 256; i++) {
		Memory_read_pages[i] = &Memory_main_pages[i];
		Memory_write_pages[i] = &Memory_main_pages[i];
	}


	for (auto i = 0; i < 256; i++) {
		m_c000_handlers[i].m_read_handler = nullptr;
		m_c000_handlers[i].m_write_handler = nullptr;
	}

	// register handlers for keyboard.   There is only a read handler here
	// since we need to read memory to get a key back from the system
	for (auto i = 0; i < 0xf; i++) {
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
		delete[] Memory_buffer;
	}

	if (Memory_extended_buffer != nullptr) {
		delete[] Memory_extended_buffer;
	}
}

