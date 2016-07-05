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

static uint8_t *Memory_buffer = nullptr;
static slot_handlers  m_c000_handlers[256];
static uint8_t last_key = 0;

uint8_t keyboard_read_handler(uint16_t addr)
{
	uint8_t temp_key = keyboard_get_key();
	if (temp_key > 0) {
		last_key = temp_key | 0x80;
	}

	return(last_key);
}

uint8_t keyboard_clear_read_handler(uint16_t addr)
{
	last_key &= 0x7F;
	return last_key;
}

void keyboard_clear_write_handler(uint16_t addr, uint8_t val)
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
   }
}


uint8_t memory_read(const uint16_t addr)
{
	static uint8_t last_key = 0;

	// look for memory mapped I/O locations
	if ((addr & 0xff00) == 0xc000) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_read_handler != nullptr ) {
			return m_c000_handlers[mapped_addr].m_read_handler(addr);
		} else {
			LOG(INFO) << "Reading from $" << std::setbase(16) << addr << " and there is no read handler.";
		}
	}
	return Memory_buffer[addr];
}

// function to write value to memory.  Trapped here in order to
// faciliate memory mapped I/O and other similar things
void memory_write(const uint16_t addr, uint8_t val)
{
	if (addr >= 0xd000) {
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

	Memory_buffer[addr] = val;
}

void memory_register_c000_handler(uint8_t addr, slot_io_read_function read_function, slot_io_write_function write_function)
{
	m_c000_handlers[addr].m_read_handler = read_function;
	m_c000_handlers[addr].m_write_handler = write_function;
}

// register handlers for the I/O slots
void memory_register_slot_handler(const uint8_t slot, slot_io_read_function read_function, slot_io_write_function write_function )
{
	assert((slot >= 1) && (slot <= MAX_SLOTS));
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

		// load rom images based on the type of machine we are starting
		memset(Memory_buffer, 0, MEMORY_SIZE);
		memory_load_rom_images();
	}

   // initialize memory with "random" pattern.  there was long discussion
   // in applewin github issues tracker related to what to do about
   // memory initialization.  https://github.com/AppleWin/AppleWin/issues/206
   memory_initialize();
   
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
}

void memory_shutdown()
{
   if (Memory_buffer != nullptr) {
      delete [] Memory_buffer;
   }
}

