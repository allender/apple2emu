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

#include <stdio.h>
#include <iomanip>

#include "apple2emu_defs.h"
#include "apple2emu.h"
#include "memory.h"
#include "video.h"
//#include "curses.h"
#include "debugger.h"
#include "keyboard.h"
#include "joystick.h"
#include "speaker.h"


// state of the memory card
uint32_t Memory_state;

// values for memory sizes
static const int Memory_main_size = (48 * 1024);
static const int Memory_rom_size = (16 * 1024);
static const int Memory_switched_bank_size = (4 * 1024);
static const int Memory_c000_rom_size = (4 * 1024);
static const int Memory_extended_size = (8 * 1024);
static const int Memory_aux_size = (48 * 256);
static const int Memory_expansion_rom_size = (2 * 1024);

static const uint16_t Memory_page_size = 256;

// and page values
static const int Memory_num_main_pages = (Memory_main_size / Memory_page_size);
static const int Memory_num_rom_pages = (Memory_rom_size / Memory_page_size);
static const int Memory_num_bank_pages = (Memory_switched_bank_size / Memory_page_size);
static const int Memory_num_internal_rom_pages = (Memory_c000_rom_size/ Memory_page_size);
static const int Memory_num_extended_pages = (Memory_extended_size / Memory_page_size);
static const int Memory_num_aux_pages = (Memory_aux_size / Memory_page_size);
static const int Memory_num_expansion_rom_pages (Memory_expansion_rom_size / Memory_page_size);

// iie roms are 16k in size.  c000 to cfff can have different
// functions depending on iie soft switches
static const int APPLE2E_ROM_SIZE = 0x4000;

// information on peripheral slots.  Note that there are really
// 8 slots in the apple.  1-7 are the normal slots but slot 0
// was used for the ram card for the apple2/2+
static const int Num_slots = 8;

static soft_switch_function m_soft_switch_handlers[256];

// class to handle memory paging.  Simple wrapper class  to hold the
// pointer and whether or not the page is write protected.  Memory
// pages will be set up when emulator starts.  Pointers to a full set
// of 64K will be stored and those pointers will point to memory_page(s)
// depending on which soft switches have been set
class memory_page {

private:
	bool     m_write_protected;
	bool     m_dirty;
	uint8_t* m_ptr;
	uint8_t  m_opcodes[Memory_page_size];

public:
	memory_page() {};

	// init sets up the pointer to the actually memory and initial
	// write protect status
	void init(uint8_t *ptr, bool write_protected) {
		m_ptr = ptr;
		m_write_protected = write_protected;
		m_dirty = true;
		for (auto i = 0; i < Memory_page_size; i++) {
			m_opcodes[i] = 0xff;
		}
	}

	// returns write protect status
	bool const write_protected() const {
		return m_write_protected;
	}

	bool const dirty() const {
		return m_dirty;
	}

	// sets write protect status
	void set_write_protected(bool write_protected) {
		m_write_protected = write_protected;
	}

	// reads the value from the given page address
	uint8_t read(const uint8_t addr, bool instruction = false) {
		uint8_t val = *(m_ptr + addr);
		if (instruction) {
			m_opcodes[addr] = val;
		}
		return val;
	}

	// get the opcode value for the given address
	uint8_t read_opcode(const uint8_t addr) { return m_opcodes[addr]; }

	// writes out a value
	void write(const uint16_t addr, uint8_t val) {
		*(m_ptr + addr) = val;
		m_dirty = true;

		// change opcode back to invalid value
		m_opcodes[addr] = 0xff;
	}
};

// main definition of memory pages for the emulator.  There is a
// page array for each "type" of memory (i.e. main, rom, extended
// auxilliary, etc).
memory_page Memory_main_pages[Memory_num_main_pages];             // 192 pages - 48k
memory_page Memory_rom_pages[Memory_num_rom_pages];               // 64 pages - 16k
memory_page Memory_bank_pages[2][Memory_num_bank_pages];          // 16 pages - 4k
memory_page Memory_extended_pages[Memory_num_extended_pages];     // 32 pages - 8k
memory_page Memory_internal_rom_pages[Memory_num_internal_rom_pages]; // 16 page - 4k
memory_page Memory_aux_pages[Memory_num_aux_pages];               // 16 page - 4k
memory_page Memory_aux_bank_pages[2][Memory_num_bank_pages];      // 16 pages - 4k
memory_page Memory_aux_extended_pages[Memory_num_extended_pages]; // 32 pages - 8k
memory_page Memory_expansion_rom_pages[Num_slots][Memory_num_expansion_rom_pages];  // 8 pages - 2k

memory_page *Memory_current_expansion_rom_pages; // this will be what is actively used

// pointers for read/write pages.  These are the methods by
// which memory reading and writing will be done.  We have
// separate read/write arrays because the apple allows writing
// to RAM banks while allowing reading from ROM area
memory_page *Memory_read_pages[Memory_page_size];
memory_page *Memory_write_pages[Memory_page_size];

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

// buffer to hold internal rom from iie rom as
static uint8_t *Memory_internal_rom_buffer = nullptr;

// buffer for the aux memory.  Includes buffers for
// bank switched RAM as well
static uint8_t *Memory_aux_buffer = nullptr;
static uint8_t *Memory_aux_bank1_buffer = nullptr;
static uint8_t *Memory_aux_bank2_buffer = nullptr;
static uint8_t *Memory_aux_extended_buffer = nullptr;

// memory buffer space for expansion rom for peripherals
static uint8_t *Memory_expansion_rom_buffer[Num_slots];

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
		fread(Memory_internal_rom_buffer, 1, Memory_c000_rom_size, fp);
		buffer_size -= Memory_c000_rom_size;
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
		memory_load_from_filename("roms/apple2e.rom", &Memory_rom_buffer[0x1000]);
	} else if (Emulator_type == emulator_type::APPLE2E_ENHANCED) {
		memory_load_from_filename("roms/apple2e_enhanced.rom", &Memory_rom_buffer[0x1000]);
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

// handler for writing to 0xc00X for memory state
uint8_t memory_set_state(uint16_t addr, uint8_t val, bool write)
{
	UNREFERENCED(val);
	UNREFERENCED(write);
	uint8_t return_val = 0;

	addr = addr & 0x0f;
	// WOOHOO -- c00X is used as keyboard read so let's just deal with that
	// here
	if (write == false) {
		return keyboard_read();
	}

	auto old_state = Memory_state;

	switch(addr) {
	case 0x00:
		Memory_state &= ~RAM_80STORE;
		break;
	case 0x01:
		Memory_state |= RAM_80STORE;
		break;
	case 0x02:
		Memory_state &= ~RAM_AUX_MEMORY_READ;
		break;
	case 0x03:
		Memory_state |= RAM_AUX_MEMORY_READ;
		break;
	case 0x04:
		Memory_state &= ~RAM_AUX_MEMORY_WRITE;
		break;
	case 0x05:
		Memory_state |= RAM_AUX_MEMORY_WRITE;
		break;
	case 0x06:
		// this is opposite of the 2e refernece manual.  Other sources
		// suggest the manual is in error
		Memory_state |= RAM_SLOTCX_ROM;
		break;
	case 0x07:
		// this is opposite of the 2e refernece manual.  Other sources
		// suggest the manual is in error
		Memory_state &= ~RAM_SLOTCX_ROM;
		break;
	case 0x08:
		Memory_state &= ~RAM_ALT_ZERO_PAGE;
		break;
	case 0x09:
		Memory_state |= RAM_ALT_ZERO_PAGE;
		break;
	case 0x0a:
		Memory_state &= ~RAM_SLOTC3_ROM;
		break;
	case 0x0b:
		Memory_state |= RAM_SLOTC3_ROM;
		break;
	}

	if (old_state != Memory_state) {
		memory_set_paging_tables();
	}

	return return_val;
}

// handler for reading memory status.  This handles 0xc010 to 0xc018
uint8_t memory_get_state(uint16_t addr, uint8_t val, bool write)
{
	uint8_t return_val = 0;

	// writes to 0xc01x reset the keyboard strobe
	if (write == true) {
		return keyboard_clear();
	}

	addr = addr & 0xff;
	switch(addr) {
	case 0x10:
		// this is the keyboard clear status
		keyboard_clear();
		break;
	case 0x11:
		return_val = Memory_state & RAM_CARD_BANK2 ? 1 : 0;
		break;
	case 0x12:
		return_val = Memory_state & RAM_CARD_READ ? 1 : 0;
		break;
	case 0x13:   // RAMRD switch
		return_val = Memory_state & RAM_AUX_MEMORY_READ ? 1 : 0;
		break;
	case 0x14:   // RAMWRT switch
		return_val = Memory_state & RAM_AUX_MEMORY_WRITE ? 1 : 0;
		break;
	case 0x15:   // SLOTCXROM switch
		// note the return values here.  0 means that slot
		// roms are active and 1 means internal rom
		return_val = Memory_state & RAM_SLOTCX_ROM ? 0 : 1;
		break;
	case 0x16:   // ALTZP switch
		return_val = Memory_state & RAM_ALT_ZERO_PAGE ? 1 : 0;
		break;
	case 0x17:   // SLOTC3ROM
		return_val = Memory_state & RAM_SLOTC3_ROM ? 1 : 0;
		break;
	case 0x18:   // 80STORE switch
		return_val = Memory_state & RAM_80STORE ? 1 : 0;
		break;
	case 0x19:
	case 0x1a:
	case 0x1b:
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
		return_val = video_get_state(addr, val, write);
		break;
	}

	if (return_val) {
		return 0x80;
	}
	return val;
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
static uint8_t memory_expansion_soft_switch_handler(uint16_t addr, uint8_t val, bool write)
{
	UNREFERENCED(write);
	UNREFERENCED(val);

	static uint8_t last_access = 0;

	addr = addr & 0xff;
	switch (addr) {

		// cases for bank 2
	case 0x80:
	case 0x84:
		Memory_state |= RAM_CARD_READ;
		Memory_state |= RAM_CARD_BANK2;
		Memory_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x81:
	case 0x85:
		Memory_state &= ~RAM_CARD_READ;
		Memory_state |= RAM_CARD_BANK2;
		Memory_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;
	case 0x82:
	case 0x86:
		Memory_state &= ~RAM_CARD_READ;
		Memory_state |= RAM_CARD_BANK2;
		Memory_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x83:
	case 0x87:
		Memory_state |= RAM_CARD_READ;
		Memory_state |= RAM_CARD_BANK2;
		Memory_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;

		// cases for bank 1
	case 0x88:
	case 0x8c:
		Memory_state |= RAM_CARD_READ;
		Memory_state &= ~RAM_CARD_BANK2;
		Memory_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x89:
	case 0x8d:
		Memory_state &= ~RAM_CARD_READ;
		Memory_state &= ~RAM_CARD_BANK2;
		Memory_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;
	case 0x8a:
	case 0x8e:
		Memory_state &= ~RAM_CARD_READ;
		Memory_state &= ~RAM_CARD_BANK2;
		Memory_state |= RAM_CARD_WRITE_PROTECT;
		last_access = 0;
		break;
	case 0x8b:
	case 0x8f:
		Memory_state |= RAM_CARD_READ;
		Memory_state &= ~RAM_CARD_BANK2;
		Memory_state &= ~RAM_CARD_WRITE_PROTECT;
		if (last_access) {
			Memory_state &= ~RAM_CARD_WRITE_PROTECT;
			last_access = 0;
		}
		else {
			last_access = 1;
		}
		break;
	default:
		ASSERT(0);
	}

	// set write protect on ramcard pages if necessary.  Total of 48 pages
	auto bank = Memory_state & RAM_CARD_BANK2 ? 1 : 0;
	for (auto i = 0; i < Memory_num_bank_pages; i++) {
		Memory_bank_pages[bank][i].set_write_protected(Memory_state & RAM_CARD_WRITE_PROTECT ? true : false);
	}
	for (auto i = 0; i < Memory_num_extended_pages; i++) {
		Memory_extended_pages[i].set_write_protected(Memory_state & RAM_CARD_WRITE_PROTECT ? true : false);
	}

	// set up paging pointers
	memory_set_paging_tables();

	return 0;
}

// set up the paging table pointers based on the current memory
// setup.
void memory_set_paging_tables()
{
	// set up the zero pages
	for (auto i = 0; i < 2; i++) {
		if (Memory_state & RAM_ALT_ZERO_PAGE) {
			Memory_read_pages[i] = &Memory_aux_pages[i];
			Memory_write_pages[i] = &Memory_aux_pages[i];
		} else {
			Memory_read_pages[i] = &Memory_main_pages[i];
			Memory_write_pages[i] = &Memory_main_pages[i];
		}
	}

	// set up the main 48k.  Skip the first two pages because
	// these will be set based on the alt zp switch (previous
	// set of code)
	for (auto page = 2; page < Memory_num_main_pages; page++) {
		if (Memory_state & RAM_AUX_MEMORY_READ) {
			Memory_read_pages[page] = &Memory_aux_pages[page];
		} else {
			Memory_read_pages[page] = &Memory_main_pages[page];
		}

		if (Memory_state & RAM_AUX_MEMORY_WRITE) {
			Memory_write_pages[page] = &Memory_aux_pages[page];
		} else {
			Memory_write_pages[page] = &Memory_main_pages[page];
		}
	}

	// set up c000 - 0xc7ff.  Set to internal rom (apple 2e) or
	// slot rom depending on slot flag
	for (auto page = 0xc0; page < 0xd0; page++) {
		if (Memory_state & RAM_SLOTCX_ROM) {
			Memory_read_pages[page] = &Memory_rom_pages[page - 0xc0];
			Memory_write_pages[page] = &Memory_rom_pages[page - 0xc0];
		} else {
			Memory_read_pages[page] = &Memory_internal_rom_pages[page - 0xc0];
			Memory_write_pages[page] = &Memory_internal_rom_pages[page - 0xc0];
		}
	}

	// check to see if slot3 page should be remapped
	if (!(Memory_state & RAM_SLOTC3_ROM)) {
		Memory_read_pages[0xc3] = &Memory_internal_rom_pages[3];
	}

	// extended RAM/ROM section.
	auto bank = Memory_state & RAM_CARD_BANK2 ? 1 : 0;

	for (auto page = 0xd0; page < 0xe0; page++) {
		if (Memory_state & RAM_CARD_READ) {
			if (Memory_state & RAM_ALT_ZERO_PAGE) {
				Memory_read_pages[page] = &Memory_aux_bank_pages[bank][page - 0xd0];
				Memory_write_pages[page] = &Memory_aux_bank_pages[bank][page - 0xd0];
			} else {
				Memory_read_pages[page] = &Memory_bank_pages[bank][page - 0xd0];
				Memory_write_pages[page] = &Memory_bank_pages[bank][page - 0xd0];
			}
		} else {
			// offset is 0xc0 here because rom pages start at page 0xc0
			Memory_read_pages[page] = &Memory_rom_pages[page - 0xc0];
			Memory_write_pages[page] = &Memory_rom_pages[page - 0xc0];
		}
	}

	for (auto page = 0xe0; page < 0x100; page++) {
		if (Memory_state & RAM_CARD_READ) {
			if (Memory_state & RAM_ALT_ZERO_PAGE) {
				Memory_read_pages[page] = &Memory_aux_extended_pages[page - 0xe0];
				Memory_write_pages[page] = &Memory_aux_extended_pages[page - 0xe0];
			} else {
				Memory_read_pages[page] = &Memory_extended_pages[page - 0xe0];
				Memory_write_pages[page] = &Memory_extended_pages[page - 0xe0];
			}
		} else {
			// offset is 0xc0 here because rom pages start at page 0xc0
			Memory_read_pages[page] = &Memory_rom_pages[page - 0xc0];
			Memory_write_pages[page] = &Memory_rom_pages[page - 0xc0];
		}
	}

	// deal with page pointers for 80 column moide
	if (Memory_state & RAM_80STORE) {
		for (auto page = 0x04; page < 0x08; page++) {
			if (Video_mode & VIDEO_MODE_PAGE2) {
				Memory_read_pages[page] = &Memory_aux_pages[page];
				Memory_write_pages[page] = &Memory_aux_pages[page];
			} else {
				Memory_read_pages[page] = &Memory_main_pages[page];
				Memory_write_pages[page] = &Memory_main_pages[page];
			}
		}

		if (Video_mode & VIDEO_MODE_HIRES) {
			for (auto page = 0x20; page < 0x40; page++) {
				if (Video_mode & VIDEO_MODE_PAGE2) {
					Memory_read_pages[page] = &Memory_aux_pages[page];
					Memory_write_pages[page] = &Memory_aux_pages[page];
				} else {
					Memory_read_pages[page] = &Memory_main_pages[page];
					Memory_write_pages[page] = &Memory_main_pages[page];
				}
			}
		}
	}
}

uint8_t memory_read_aux(const uint16_t addr)
{
	auto page = (addr / Memory_page_size);
	return Memory_aux_pages[page].read(addr & 0xff);
}

uint8_t memory_read_main(const uint16_t addr)
{
	auto page = (addr / Memory_page_size);
	return Memory_main_pages[page].read(addr & 0xff);
}

uint8_t memory_read(const uint16_t addr, bool instruction)
{
	auto page = (addr / Memory_page_size);

	// look for memory mapped I/O locations
	if (page == 0xc0) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_soft_switch_handlers[mapped_addr] != nullptr) {
			return m_soft_switch_handlers[mapped_addr](addr, 0, false);
		}
		if (Memory_read_pages[page] == nullptr) {
			return 0;
		}
	}

	// reset rom expansion page settings
	if (Emulator_type >= emulator_type::APPLE2E) {
		if (addr == 0xcfff) {
			// read to 0xcfff resets the expansion rom area
			Memory_state |= RAM_EXPANSION_RESET;
			Memory_current_expansion_rom_pages = nullptr;
		}

		// handle reads in perhiperal rom (or internal rom) memory
		else if (page >= 0xc1 && page <= 0xc7 && Memory_current_expansion_rom_pages == nullptr) {

			// check to see if we need to update the expansion ROM area.  Access
			// to a peripheral slot means that we _might_ access the expansion
			// rom.  Set flags to indicate what pages might need to be paged
			// in if that expansion area is accessed.
			auto slot = (page & 0xf);
			if (Memory_expansion_rom_buffer[slot] != nullptr) {
				Memory_current_expansion_rom_pages = &Memory_expansion_rom_pages[slot][0];
			}
			else if (slot == 3 && !(Memory_state & RAM_SLOTC3_ROM)) {
				Memory_current_expansion_rom_pages = &Memory_internal_rom_pages[0x8];
			} else {
				Memory_current_expansion_rom_pages = &Memory_internal_rom_pages[0x8];
			}
		}

		// we are accessing expansion rom.  (apple 2e only).  When we
		// access these pages, we need to make sure that the page pointers
		// point to the correct location for the expansion rom
		else if (page >= 0xc8 && page <= 0xcf) {
			// if we haven't paged in the memory pages yet, do so here.  If
			// the reset flag is set, and no slot is active, then we can
			// just use the internal rom.  If reset is active and a slot
			// is active, use the slot's expansion rom (or internal rom
			// if this is slot 3)
			if (Memory_state & RAM_EXPANSION_RESET) {
				if (Memory_current_expansion_rom_pages == nullptr) {
					Memory_current_expansion_rom_pages = &Memory_internal_rom_pages[0x8];
				}
				// this is a reset case and we just point to the internal rom pages
				for (auto i = 0xc8; i <= 0xcf; i++) {
					Memory_read_pages[i] = &Memory_current_expansion_rom_pages[i - 0xc8];
				}
				Memory_state &= ~RAM_EXPANSION_RESET;
			}
		}
	} else {
		// apple2 plus, is there is no expansion rom.  If there is no
		// handler for the page, then just return
		if (page >= 0xc0 && page <= 0xcf) {
			if (Memory_read_pages[page] == nullptr) {
				return 0;
			}
		}
	}

	ASSERT(Memory_read_pages[page] != nullptr);
	return Memory_read_pages[page]->read(addr & 0xff, instruction);
}

// finds the nth previous opcode from the current
// address.
uint16_t memory_find_previous_opcode_addr(const uint16_t addr, int num)
{
	auto page = (addr / Memory_page_size);
	uint8_t mapped_addr = addr & 0xff;
	uint8_t last_valid_address = mapped_addr;
	auto last_valid_page = page;
	int num_invalid = 0;
	while (page >= 0) {
		for (uint8_t a = mapped_addr - 1; a !=0xff; a--) {
			uint8_t opcode = Memory_read_pages[page]->read_opcode(a);
			if (opcode != 0xff) {
				num--;
				last_valid_address = a;
				last_valid_page = page;
				num_invalid = 0;
				if (num == 0) {
					return (uint16_t)((page * 256) + a);
				}
			}

			// if we get 4 invalid opcodes, then we can't proceed any further
			// back so return the last known good address that we had.
			else {
				num_invalid++;
				if (num_invalid == 4) {
					return (uint16_t)((last_valid_page * 256) + last_valid_address);
				}
			}
		}
		// go to previous page to the last memory location
		page--;
		mapped_addr = 0xff;
	}

	// ugh - we get here, then we have nothing, which would be really
	// bad.  So let's return the largest value and disassembler
	// can just deal with it there
	return 0xffff;
}

// function to write value to memory.  Trapped here in order to
// faciliate memory mapped I/O and other similar things
void memory_write(const uint16_t addr, uint8_t val)
{
	auto page = (addr / Memory_page_size);

	if (page == 0xc0) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_soft_switch_handlers[mapped_addr] != nullptr) {
			m_soft_switch_handlers[mapped_addr](addr, val, true);
			return;
		}
		if (Memory_write_pages[page] == nullptr) {
			return;
		}
	}

	ASSERT(Memory_write_pages[page] != nullptr);
	if (Memory_write_pages[page]->write_protected()) {
		return;
	}

	Memory_write_pages[page]->write(addr & 0xff, val);
}

void memory_register_soft_switch_handler(const uint8_t addr, soft_switch_function func)
{
	m_soft_switch_handlers[addr] = func;
}

// register handlers for the I/O slots
void memory_register_slot_handler(const uint8_t slot, soft_switch_function func, uint8_t *expansion_rom)
{
	ASSERT((slot >= 0) && (slot < Num_slots));
	uint8_t addr = 0x80 + (slot << 4);

	// add handlers for the slot.  There are 16 addresses per slot so set them all to the same
	// handler as we will deal with all slot operations in the same function
	for (auto i = 0; i <= 0xf; i++) {
		m_soft_switch_handlers[addr + i] = func;
	}

	if (expansion_rom != nullptr) {
		Memory_expansion_rom_buffer[slot] = expansion_rom;
	}
}

bool memory_load_buffer(uint8_t *buffer, uint16_t size, uint16_t location)
{
	// move the buffer into memory.  The problem here is that
	// the memory pages have already been set up, including
	// read/write status of the pages
	for (int addr = location; addr < location + size; addr++) {
		auto page = (addr / Memory_page_size);
		Memory_write_pages[page]->write(addr & 0xff, buffer[addr - location]);
	}
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
	if (Memory_internal_rom_buffer == nullptr) {
		Memory_internal_rom_buffer = new uint8_t[Memory_c000_rom_size];
	}
	memset(Memory_internal_rom_buffer, 0, Memory_c000_rom_size);

	// load rom images based on the type of machine we are starting
	memory_load_rom_images();

	// allocate ram for banks in extended ram
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

	// alternate zero page buffer
	if (Memory_aux_buffer == nullptr) {
		Memory_aux_buffer = new uint8_t[Memory_aux_size];
	}
	memset(Memory_aux_buffer, 0, Memory_aux_size);

	// and banks for aux memory
	if (Memory_aux_bank1_buffer == nullptr) {
		Memory_aux_bank1_buffer = new uint8_t[Memory_switched_bank_size];
	}
	if (Memory_aux_bank2_buffer == nullptr) {
	  Memory_aux_bank2_buffer = new uint8_t[Memory_switched_bank_size];
	}
	memset(Memory_aux_bank1_buffer, 0, Memory_switched_bank_size);
	memset(Memory_aux_bank2_buffer, 0, Memory_switched_bank_size);

	// This is the 12k buffer that is active for either bank
	// 1 or bank 2
	if (Memory_aux_extended_buffer == nullptr) {
	  Memory_aux_extended_buffer = new uint8_t[Memory_extended_size];
	}
	memset(Memory_aux_extended_buffer, 0, Memory_extended_size);

	for (auto i = 0; i < Num_slots; i++) {
		Memory_expansion_rom_buffer[i] = nullptr;
	}

	// initialize memory with "random" pattern.  there was long discussion
	// in applewin github issues tracker related to what to do about
	// memory initialization.  https://github.com/AppleWin/AppleWin/issues/206
	memory_initialize();

	// main memory area page pointers.  This is not write protected.  There
	// are
	for (auto i = 0; i < Memory_num_main_pages; i++) {
		Memory_main_pages[i].init(&Memory_buffer[i * Memory_page_size], false);
	}

	// ROM area.  This is write protected
	for (auto i = 0; i < Memory_num_rom_pages; i++) {
		Memory_rom_pages[i].init(&Memory_rom_buffer[i * Memory_page_size], true);
	}

	// set up pointers for ramcard pages
	for (auto i = 0; i < Memory_num_bank_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_bank_pages[0][i].init(&Memory_bank1_buffer[addr], false);
		Memory_bank_pages[1][i].init(&Memory_bank2_buffer[addr], false);
	}

	// final 12k, the page pointers are the same.  Offset 8k into extended memory
	// buffer because we have 2 4k pages before that
	for (auto i = 0; i < Memory_num_extended_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_extended_pages[i].init(&Memory_extended_buffer[addr], false);
	}

	// set up pages for internal ROM on apple iie
	for (auto i = 0; i < Memory_num_internal_rom_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_internal_rom_pages[i].init(&Memory_internal_rom_buffer[addr], true);
	}

	// memory for the auxiliary ram
	for (auto i = 0; i < Memory_num_aux_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_aux_pages[i].init(&Memory_aux_buffer[addr], false);
	}

	// set up pointers for auxiliary bank pages
	for (auto i = 0; i < Memory_num_bank_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_aux_bank_pages[0][i].init(&Memory_aux_bank1_buffer[addr], false);
		Memory_aux_bank_pages[1][i].init(&Memory_aux_bank2_buffer[addr], false);
	}

	// final 12k of exnteded RAM in the auxiliary buffer
	for (auto i = 0; i < Memory_num_extended_pages; i++) {
		uint32_t addr = i * Memory_page_size;
		Memory_aux_extended_pages[i].init(&Memory_aux_extended_buffer[addr], false);
	}

	// set up the main memory card state.  Make sure to set the
	// reset expansion rom flag so that the expansion rom gets
	// reset to the internal rom (for the apple2e)
	Memory_state = RAM_CARD_BANK2 | RAM_SLOTCX_ROM | RAM_EXPANSION_RESET;
	if (Emulator_type < emulator_type::APPLE2E) {
		Memory_state |= RAM_CARD_WRITE_PROTECT;
	}

	// update paging based on the current memory configuration.  this
	// will set up pointers to the memory pages to point to the
	// correct pages
	memory_set_paging_tables();

	for (auto i = 0; i < Memory_num_main_pages; i++) {
		Memory_read_pages[i] = &Memory_main_pages[i];
		Memory_write_pages[i] = &Memory_main_pages[i];
	}

	for (auto i = 0; i < 256; i++) {
		m_soft_switch_handlers[i] = nullptr;
	}

	// register handlers for 0xc000 to 0xc00c.  These are memory
	// management switches (except for the read of 0xc000 which is
	// reading the keyboard).
	for (uint8_t i = 0; i < 0x0c; i++) {
		memory_register_soft_switch_handler(i, memory_set_state);
	}
	for (uint8_t i = 0x0c; i < 0x10; i++) {
		memory_register_soft_switch_handler(i, video_set_state);
	}

	// register handler for reading memory status
	for (uint8_t i = 0x10; i < 0x20; i++) {
		memory_register_soft_switch_handler(i, memory_get_state);
	}

	// register 0xc030 for the speaker
	memory_register_soft_switch_handler(0x30, speaker_soft_switch_handler);

	// set up memory handlers for video
	for (uint8_t i = 0x50; i <= 0x57; i++) {
		memory_register_soft_switch_handler(i, video_set_state);
	}

	// register the read/write handlers for the joystick
	for (uint8_t i = 0x61; i < 0x67; i++) {
		memory_register_soft_switch_handler(i, joystick_soft_switch_handler);
	}
	memory_register_soft_switch_handler(0x70, joystick_soft_switch_handler);

	// register a handler for memory expansion card in slot 0.  This handler will
	// also handle the first 64K in the apple iie
	memory_register_slot_handler(0, memory_expansion_soft_switch_handler);
}

void memory_shutdown()
{
	if (Memory_buffer != nullptr) {
		delete[] Memory_buffer;
	}
	if (Memory_rom_buffer != nullptr) {
		delete[] Memory_rom_buffer;
	}
	if (Memory_internal_rom_buffer != nullptr) {
		delete[] Memory_internal_rom_buffer;
	}
	if (Memory_bank1_buffer != nullptr) {
		delete[] Memory_bank1_buffer;
	}
	if (Memory_bank2_buffer != nullptr) {
		delete[] Memory_bank2_buffer;
	}
	if (Memory_extended_buffer != nullptr) {
		delete[] Memory_extended_buffer;
	}
	if (Memory_aux_buffer != nullptr) {
		delete[] Memory_aux_buffer;
	}
	if (Memory_aux_bank1_buffer != nullptr) {
		delete[] Memory_aux_bank1_buffer;
	}
	if (Memory_aux_bank2_buffer != nullptr) {
		delete[] Memory_aux_bank2_buffer;
	}
	if (Memory_aux_extended_buffer != nullptr) {
		delete[] Memory_aux_extended_buffer;
	}
}

