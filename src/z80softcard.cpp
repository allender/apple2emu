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

/*
 *  Code for Z80, which is used in the z80 softcard for the Apple ][.
 *  The card provides CP/M compatibility.
 */

#include "apple2emu_defs.h"
#include "z80softcard.h"
#include "memory.h"
#include "../z80emu/z80emu.h"

/*
 * basic interface into the z80 emulator code.  Since I have used
 * an external emulator, this code is the glue layer between
 * the apple ][ emulator and the z80 emulator.
*/

enum class z80_state {
	WAIT,
	ACTIVE,
};

static z80_state Z80_state;

static bool Map_memory = true;

static uint16_t z80_map_z80_to_6502(uint16_t addr)
{
	if (Map_memory == false) {
		return addr;
	}

	uint8_t h = addr >> 12;
	uint16_t return_addr = addr;

	if (h < 0xb) {
		return_addr += 0x1000;
	} else if (h >= 0xb && h <= 0xd) {
		return_addr += 0x2000;
	} else if (h == 0xe) {
		return_addr -= 0x2000;
	} else {
		return_addr -= 0xf000;
	}

	return return_addr;
}

int z80softcard_emulate(Z80_STATE *z80_cpu, int number_cycles)
{
	if (Z80_state == z80_state::WAIT) {
		return 0;
	}
	return Z80Emulate(z80_cpu, number_cycles, nullptr);
}

uint8_t z80_memory_read(uint16_t addr)
{
	uint16_t mapped_addr = z80_map_z80_to_6502(addr);
	return memory_read(mapped_addr);
}

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
void z80_memory_write(uint16_t addr, uint8_t val)
{
	uint16_t mapped_addr = z80_map_z80_to_6502(addr);
	memory_write(mapped_addr, val);
}

// handler for read/writes to z80 softcard space
static uint8_t z80_handler(uint16_t addr, uint8_t val, bool write)
{
	UNREFERENCED(addr);
	UNREFERENCED(val);
	if (write) {
		Z80_state = (Z80_state == z80_state::WAIT ? z80_state::ACTIVE : z80_state::WAIT);
	}
	return 255;
}

// initialize the z80 softward aystem
void z80softcard_init()
{
	memory_register_slot_memory_handler(4, z80_handler);
}

void z80softcard_reset(Z80_STATE *z80_cpu)
{
	Z80Reset(z80_cpu);
}

