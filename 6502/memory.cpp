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

#define MEMORY_SIZE (64 * 1024 * 1024)
#define MEMORY_BANK_SIZE  (4 * 1024)
#define MEMORY_EXTENDED_SIZE  (12 * 1024)

#define MAX_SLOTS 7

// defines for memory status (RAM card, 0xc000 usage, etc)
#define RAM_CARD_READ           (1 << 0)
#define RAM_CARD_BANK2          (1 << 1)
#define RAM_CARD_WRITE_PROTECT  (1 << 2)
#define RAM_SLOTCX_ROM          (1 << 3)
#define RAM_SLOTC3_ROM          (1 << 4)

// iie roms are 16k in size.  c000 to cfff can have different
// functions depending on iie soft switches
static const int APPLE2E_ROM_SIZE = 0x4000;
static const int C000_ROM_SIZE = 0x1000;

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
   void set_ptr(uint8_t *ptr) { m_ptr = ptr; }
};

// definitions for memory paging.  There will be one set of memory_page(s)
// for the emulator and the pointers for those pages will be changed
// depending on how memory is configured with the soft switches

// main definition of memory pages for the emulator
memory_page Memory_main_pages[256];    // 256 pages of 256 bytes/page for 64k
memory_page Memory_bank1_pages[48];    // 48 pages in bank 1
memory_page Memory_bank2_pages[48];    // 48 pages in bank 2

memory_page *Memory_read_pages[256];   // point to pages which can be read
memory_page *Memory_write_pages[256];  // point to pages which can be written

// Below are the definitions for the memory buffers.  These buffers
// will contain the memory for the various paging setups that can
// happen in the emulator.  Code will use the addresses within these
// buffers to set up the memory page pointers appropriately

// main buffer of 64k.
static uint8_t *Memory_buffer = nullptr;

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

static bool memory_load_from_filename(const char *filename, uint16_t location)
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
		location += 0x1000;
		buffer_size -= 0x1000;
	}

	// allocate the memory buffer
	fread(&Memory_buffer[location], 1, buffer_size, fp);
	fclose(fp);
	return true;
}

// loads up appropriate rom images which is based on the
// type of machine we are trying to initialize
static void memory_load_rom_images()
{
	if (Emulator_type == emulator_type::APPLE2) {
		memory_load_from_filename("roms/apple2.rom", 0xd000);
	}
	else if (Emulator_type == emulator_type::APPLE2_PLUS) {
		memory_load_from_filename("roms/apple2_plus.rom", 0xd000);
	} else if (Emulator_type == emulator_type::APPLE2E) {
		memory_load_from_filename("roms/apple2e.rom", 0xc000);
	}
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
   }

	// same with bank  buffers
   for (auto i = 0; i < MEMORY_BANK_SIZE; i += 4) {
		Memory_bank1_buffer[i] = 0xff;
		Memory_bank1_buffer[i + 1] = 0xff;
		Memory_bank2_buffer[i] = 0xff;
		Memory_bank2_buffer[i + 1] = 0xff;
	}

	// same with extended buffer
   for (auto i = 0; i < MEMORY_EXTENDED_SIZE; i += 4) {
		Memory_extended_buffer[i] = 0xff;
		Memory_extended_buffer[i + 1] = 0xff;
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
		}
		else {
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
		}
		else {
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
		}
		else {
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
		}
		else {
			last_access = 1;
		}
	}

	// set write protect on ramcard pages if necessary.  There are a
   // total of 48 pages
	for (auto i = 0; i < 48; i++) {
		if (Memory_card_state & RAM_CARD_BANK2) {
			Memory_bank2_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT ? true : false);
		}
		else {
			Memory_bank1_pages[i].set_write_protected(Memory_card_state & RAM_CARD_WRITE_PROTECT ? true : false);
		}
	}

	// set the current set of pages being used
	if (Memory_card_state & RAM_CARD_READ) {
		if (Memory_card_state & RAM_CARD_BANK2) {
         for (auto i = 0xd0; i < 0x100; i++) {
            Memory_read_pages[i] = &Memory_bank2_pages[i - 0xd0];
            Memory_write_pages[i] = &Memory_bank2_pages[i - 0xd0];
         }
		}
		else {
         for (auto i = 0xd0; i < 0x100; i++ ) {
            Memory_read_pages[i] = &Memory_bank1_pages[i - 0xd0];
            Memory_write_pages[i] = &Memory_bank1_pages[i - 0xd0];
         }
		}
	}
	else {
      for (auto i = 0xd0; i < 0x100; i++) {
         Memory_read_pages[i] = &Memory_main_pages[i];
         Memory_write_pages[i] = &Memory_main_pages[i];
      }
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

	if (Memory_write_pages[page]->write_protected()) {
		return;
	}

	if ((addr & 0xff00) == 0xc000) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_write_handler != nullptr) {
			m_c000_handlers[mapped_addr].m_write_handler(addr, val);
			return;
		}
		else {
			//LOG(INFO) << "Writing $" << std::setbase(16) << std::setw(2) << val << " to $" << std::setbase(16) << addr << " and there is no write handler";
		}
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
	// main memory buffer - 64k
	if (Memory_buffer == nullptr) {
		Memory_buffer = new uint8_t[MEMORY_SIZE];
	}
	memset(Memory_buffer, 0, MEMORY_SIZE);

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
		Memory_bank1_buffer = new uint8_t[MEMORY_BANK_SIZE];
	}
   if (Memory_bank2_buffer == nullptr) {
      Memory_bank2_buffer = new uint8_t[MEMORY_BANK_SIZE];
   }
   memset(Memory_bank1_buffer, 0, MEMORY_BANK_SIZE);
   memset(Memory_bank2_buffer, 0, MEMORY_BANK_SIZE);

   // This is the 12k buffer that is active for either bank
   // 1 or bank 2
   if (Memory_extended_buffer == nullptr) {
      Memory_extended_buffer = new uint8_t[MEMORY_EXTENDED_SIZE];
   }
	memset(Memory_extended_buffer, 0, MEMORY_EXTENDED_SIZE);

	// initialize memory with "random" pattern.  there was long discussion
	// in applewin github issues tracker related to what to do about
	// memory initialization.  https://github.com/AppleWin/AppleWin/issues/206
	memory_initialize();

   // main memory area page pointers.  This is not write
   // protected
	for (auto i = 0; i < 0xd0; i++) {
		Memory_main_pages[i].init(&Memory_buffer[i * 256], false);
	}

   // ROM area.  This is write protected
	for (auto i = 0xd0; i < 0x100; i++) {
		Memory_main_pages[i].init(&Memory_buffer[i * 256], true);
	}

   // set up pointers for ramcard pages
   for (auto i = 0; i < 16; i++) {
      uint32_t addr = i * 256;
      Memory_bank1_pages[i].init(&Memory_bank1_buffer[addr], true);
      Memory_bank2_pages[i].init(&Memory_bank2_buffer[addr], true);
   }
   // final 12k, the page pointers are the same.  Offset 8k into extended memory
   // buffer because we have 2 4k pages before that
   for (auto i = 0; i < 32; i++) {
      uint32_t addr = i * 256;
      Memory_bank1_pages[i + 16].init(&Memory_extended_buffer[addr], true);
      Memory_bank2_pages[i + 16].init(&Memory_extended_buffer[addr], true);
   }

   // set the active pages to point to the correct startup locations
   // which is just the 256 pages of the main set of pages
   for (auto i = 0; i < 256; i++) {
      Memory_read_pages[i] = &Memory_main_pages[i];
      Memory_write_pages[i] = &Memory_main_pages[i];
   }

	Memory_card_state = 0;
	Memory_card_state = RAM_CARD_BANK2 | RAM_SLOTCX_ROM;

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

