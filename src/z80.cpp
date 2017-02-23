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
#include "z80.h"

// opcodes for the z80
z80::opcode_info z80::m_opcodes[] = {
	// 0x00 - 0x0f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x10 - 0x1f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x20 - 0x2f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x30 - 0x3f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x40 - 0x4f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x50 - 0x5f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x60 - 0x6f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x70 - 0x7f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER_INDIRECT, &z80::no_mode },
	{ 'LD  ', 0, 0, addr_mode::REGISTER, &z80::no_mode },

	// 0x80 - 0x8f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0x90 - 0x9f
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xa0 - 0xaf
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xb0 - 0xbf
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xc0 - 0xcf
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xd0 - 0xdf
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xe0 - 0xef
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

	// 0xf0 - 0xff
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },
	{ '    ', 0, 0, addr_mode::NONE, &z80::no_mode },

};


int16_t z80::no_mode()
{
	return 0;
}

uint8_t z80_memory_read(uint16_t addr)
{
	UNREFERENCED(addr);
	return 0;
}

void z80_memory_write(const uint16_t addr, const uint8_t val)
{
	UNREFERENCED(addr);
	UNREFERENCED(val);
}

// 8 bit load from register to register.
void z80::load_register(int reg, uint8_t val)
{
	m_registers.bytes[reg] = val;
}

void z80::store_immediate(uint16_t addr, uint8_t val)
{
	z80_memory_write(addr, val);
}

void z80::store_register(uint16_t addr, int reg)
{
	store_immediate(addr, m_registers.bytes[reg]);
}

// pop values off stack into given register
void z80::pop(int reg)
{
	uint16_t val = z80_memory_read(m_registers.shorts[SP_INDEX]++) << 8 |
		z80_memory_read(m_registers.shorts[SP_INDEX]++);
	m_registers.shorts[reg] = val;
}

// pushes the given register value onto the stack
void z80::push(int reg)
{
	z80_memory_write(m_registers.shorts[SP_INDEX], m_registers.shorts[reg] >> 8 & 0xff);
	m_registers.shorts[SP_INDEX]--;
	z80_memory_write(m_registers.shorts[SP_INDEX]--, m_registers.shorts[reg] & 0xff);
	m_registers.shorts[SP_INDEX]--;
}

void z80::add(uint8_t val, int carry)
{
	uint8_t a_reg = m_registers.bytes[A_INDEX];

	int tmp = val + a_reg + carry;
	set_flag(register_bit::CARRY_BIT, tmp > 0x100);
	set_flag(register_bit::HALF_CARRY_BIT, (a_reg ^ val ^ tmp) &
		(1<<static_cast<uint8_t>(register_bit::HALF_CARRY_BIT)));
	set_flag(register_bit::PARITY_BIT, ~(a_reg ^ val) & (a_reg ^ tmp) & 0x80);
	m_registers.bytes[A_INDEX] = tmp & 0xff;
}

void z80::sub(uint8_t val, int carry)
{
	uint8_t a_reg = m_registers.bytes[A_INDEX];
	int tmp = m_registers.bytes[A_INDEX] - val - carry;
	set_flag(register_bit::CARRY_BIT, val > a_reg);
	set_flag(register_bit::HALF_CARRY_BIT, a_reg ^ val ^ tmp &
		(1<<static_cast<uint8_t>(register_bit::HALF_CARRY_BIT)));
	set_flag(register_bit::PARITY_BIT, (a_reg ^ val) & (a_reg ^ tmp) & 0x80);
	m_registers.bytes[A_INDEX] = tmp & 0xff;
}

void z80::op_and(uint8_t val)
{
	m_registers.bytes[A_INDEX] &= val;
}

void z80::op_xor(uint8_t val)
{
	m_registers.bytes[A_INDEX] ^= val;
}

void z80::op_or(uint8_t val)
{
	m_registers.bytes[A_INDEX] |= val;
}

void z80::cp(uint8_t val)
{
	UNREFERENCED(val);
}

void z80::dec_register(int reg)
{
	m_registers.bytes[reg]--;
}

void z80::dec_indirect(uint16_t addr)
{
	uint8_t val = z80_memory_read(addr);
	val--;
	z80_memory_write(addr, val);
}

void z80::inc_register(int reg)
{
	m_registers.bytes[reg]++;
}

void z80::inc_indirect(uint16_t addr)
{
	uint8_t val = z80_memory_read(addr);
	val++;
	z80_memory_write(addr, val);
}

// process a z80 opcode.  Returns the number of cycles that were
// executed for the processed opcode
uint32_t z80::process_opcode()
{
	uint8_t opcode = z80_memory_read(m_pc++);

	switch(opcode) {

	case 0x01:
		m_registers.bytes[C_INDEX] = z80_memory_read(m_pc + 1);
		m_registers.bytes[B_INDEX] = z80_memory_read(m_pc + 2);
		break;
	case 0x02:
		store_register(m_registers.shorts[BC_INDEX], A_INDEX);
		break;
	case 0x04:
		inc_register(B_INDEX);
		break;
	case 0x06:
		load_register(B_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x08:
	{
		uint16_t tmp = m_alternate_regs[AF_INDEX];
		m_alternate_regs[AF_INDEX] = m_registers.shorts[AF_INDEX];
		m_registers.shorts[AF_INDEX] = tmp;
	}
	case 0x0a:
		load_register(A_INDEX, z80_memory_read(m_registers.shorts[BC_INDEX]));
		break;
	case 0x0c:
		inc_register(C_INDEX);
		break;
	case 0x0e:
		load_register(C_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x11:
		m_registers.bytes[E_INDEX] = z80_memory_read(m_pc + 1);
		m_registers.bytes[D_INDEX] = z80_memory_read(m_pc + 2);
		break;
	case 0x12:
		store_register(m_registers.shorts[DE_INDEX], A_INDEX);
		break;
	case 0x14:
		inc_register(D_INDEX);
		break;
	case 0x16:
		load_register(D_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x1a:
		load_register(A_INDEX, z80_memory_read(m_registers.shorts[DE_INDEX]));
		break;
	case 0x1c:
		inc_register(E_INDEX);
		break;
	case 0x1e:
		load_register(E_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x21:
		m_registers.bytes[L_INDEX] = z80_memory_read(m_pc + 1);
		m_registers.bytes[H_INDEX] = z80_memory_read(m_pc + 2);
		break;
	case 0x22:
	{
		uint16_t addr = z80_memory_read(m_pc + 1) << 8 | z80_memory_read(m_pc + 2);
		z80_memory_write(addr, m_registers.bytes[L_INDEX]);
		z80_memory_write(addr + 1, m_registers.bytes[H_INDEX]);
		break;
	}
	case 0x24:
		inc_register(F_INDEX);
		break;
	case 0x26:
		load_register(H_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x2c:
		inc_register(L_INDEX);
		break;
	case 0x2e:
		load_register(L_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x31:
		m_registers.bytes[SPL_INDEX] = z80_memory_read(m_pc + 1);
		m_registers.bytes[SPH_INDEX] = z80_memory_read(m_pc + 2);
		break;
	case 0x32:
	{
		uint16_t addr = z80_memory_read(m_pc + 1) << 8 | z80_memory_read(m_pc + 2);
		store_register(addr, A_INDEX);
		break;
	}
	case 0x34:
		inc_indirect(z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0x35:
		dec_indirect(z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0x36:
		store_immediate(m_registers.shorts[HL_INDEX], z80_memory_read(m_pc + 1));
		break;
	case 0x3a:
	{
		uint16_t mem_loc = z80_memory_read(m_pc + 1) << 8 | z80_memory_read(m_pc + 2);
		load_register(A_INDEX, z80_memory_read(mem_loc));
		break;
	}
	case 0x3c:
		inc_register(A_INDEX);
		break;
	case 0x3e:
		load_register(A_INDEX, z80_memory_read(m_pc + 1));
		break;
	case 0x40:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x41:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x42:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x43:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x44:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x45:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x47:
		load_register(B_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x48:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x49:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x4a:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x4b:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x4c:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x4d:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x4f:
		load_register(C_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x50:
		load_register(D_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x51:
		load_register(D_INDEX, m_registers.bytes[C_INDEX]);
		break;
	case 0x52:
		load_register(D_INDEX, m_registers.bytes[D_INDEX]);
		break;
	case 0x53:
		load_register(D_INDEX, m_registers.bytes[E_INDEX]);
		break;
	case 0x54:
		load_register(D_INDEX, m_registers.bytes[F_INDEX]);
		break;
	case 0x55:
		load_register(D_INDEX, m_registers.bytes[L_INDEX]);
		break;
	case 0x57:
		load_register(D_INDEX, m_registers.bytes[A_INDEX]);
		break;
	case 0x58:
		load_register(E_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x59:
		load_register(E_INDEX, m_registers.bytes[C_INDEX]);
		break;
	case 0x5a:
		load_register(E_INDEX, m_registers.bytes[D_INDEX]);
		break;
	case 0x5b:
		load_register(E_INDEX, m_registers.bytes[E_INDEX]);
		break;
	case 0x5c:
		load_register(E_INDEX, m_registers.bytes[F_INDEX]);
		break;
	case 0x5d:
		load_register(E_INDEX, m_registers.bytes[L_INDEX]);
		break;
	case 0x5f:
		load_register(E_INDEX, m_registers.bytes[A_INDEX]);
		break;
	case 0x60:
		load_register(H_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x61:
		load_register(H_INDEX, m_registers.bytes[C_INDEX]);
		break;
	case 0x62:
		load_register(H_INDEX, m_registers.bytes[D_INDEX]);
		break;
	case 0x63:
		load_register(H_INDEX, m_registers.bytes[E_INDEX]);
		break;
	case 0x64:
		load_register(H_INDEX, m_registers.bytes[F_INDEX]);
		break;
	case 0x65:
		load_register(H_INDEX, m_registers.bytes[L_INDEX]);
		break;
	case 0x67:
		load_register(H_INDEX, m_registers.bytes[A_INDEX]);
		break;
	case 0x68:
		load_register(L_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x69:
		load_register(L_INDEX, m_registers.bytes[C_INDEX]);
		break;
	case 0x6a:
		load_register(L_INDEX, m_registers.bytes[D_INDEX]);
		break;
	case 0x6b:
		load_register(L_INDEX, m_registers.bytes[E_INDEX]);
		break;
	case 0x6c:
		load_register(L_INDEX, m_registers.bytes[F_INDEX]);
		break;
	case 0x6d:
		load_register(L_INDEX, m_registers.bytes[L_INDEX]);
		break;
	case 0x6f:
		load_register(L_INDEX, m_registers.bytes[A_INDEX]);
		break;
	case 0x70:
		store_register(m_registers.shorts[HL_INDEX], B_INDEX);
		break;
	case 0x71:
		store_register(m_registers.shorts[HL_INDEX], C_INDEX);
		break;
	case 0x72:
		store_register(m_registers.shorts[HL_INDEX], D_INDEX);
		break;
	case 0x73:
		store_register(m_registers.shorts[HL_INDEX], E_INDEX);
		break;
	case 0x74:
		store_register(m_registers.shorts[HL_INDEX], F_INDEX);
		break;
	case 0x75:
		store_register(m_registers.shorts[HL_INDEX], L_INDEX);
		break;
	case 0x77:
		store_register(m_registers.shorts[HL_INDEX], A_INDEX);
		break;
	case 0x78:
		load_register(A_INDEX, m_registers.bytes[B_INDEX]);
		break;
	case 0x79:
		load_register(A_INDEX, m_registers.bytes[C_INDEX]);
		break;
	case 0x7a:
		load_register(A_INDEX, m_registers.bytes[D_INDEX]);
		break;
	case 0x7b:
		load_register(A_INDEX, m_registers.bytes[E_INDEX]);
		break;
	case 0x7c:
		load_register(A_INDEX, m_registers.bytes[F_INDEX]);
		break;
	case 0x7d:
		load_register(A_INDEX, m_registers.bytes[L_INDEX]);
		break;
	case 0x7e:
		load_register(A_INDEX, z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0x7f:
		load_register(A_INDEX, m_registers.bytes[A_INDEX]);
		break;
	case 0x80:
		add(m_registers.bytes[B_INDEX], 0);
		break;
	case 0x81:
		add(m_registers.bytes[C_INDEX], 0);
		break;
	case 0x82:
		add(m_registers.bytes[D_INDEX], 0);
		break;
	case 0x83:
		add(m_registers.bytes[E_INDEX], 0);
		break;
	case 0x84:
		add(m_registers.bytes[F_INDEX], 0);
		break;
	case 0x85:
		add(m_registers.bytes[L_INDEX], 0);
		break;
	case 0x86:
		add(z80_memory_read(m_registers.shorts[HL_INDEX]), 0);
		break;
	case 0x87:
		add(m_registers.bytes[A_INDEX], 0);
		break;
	case 0x88:
		add(m_registers.bytes[B_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x89:
		add(m_registers.bytes[C_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8a:
		add(m_registers.bytes[D_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8b:
		add(m_registers.bytes[E_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8c:
		add(m_registers.bytes[F_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8d:
		add(m_registers.bytes[L_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8e:
		add(z80_memory_read(m_registers.shorts[HL_INDEX]), get_flag(register_bit::CARRY_BIT));
		break;
	case 0x8f:
		add(m_registers.bytes[A_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x90:
		sub(m_registers.bytes[B_INDEX], 0);
		break;
	case 0x91:
		sub(m_registers.bytes[C_INDEX], 0);
		break;
	case 0x92:
		sub(m_registers.bytes[D_INDEX], 0);
		break;
	case 0x93:
		sub(m_registers.bytes[E_INDEX], 0);
		break;
	case 0x94:
		sub(m_registers.bytes[F_INDEX], 0);
		break;
	case 0x95:
		sub(m_registers.bytes[L_INDEX], 0);
		break;
	case 0x96:
		sub(z80_memory_read(m_registers.shorts[HL_INDEX]), 0);
		break;
	case 0x97:
		sub(m_registers.bytes[A_INDEX], 0);
		break;
	case 0x98:
		sub(m_registers.bytes[B_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x99:
		sub(m_registers.bytes[C_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9a:
		sub(m_registers.bytes[D_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9b:
		sub(m_registers.bytes[E_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9c:
		sub(m_registers.bytes[F_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9d:
		sub(m_registers.bytes[L_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9e:
		sub(z80_memory_read(m_registers.shorts[HL_INDEX]), get_flag(register_bit::CARRY_BIT));
		break;
	case 0x9f:
		sub(m_registers.bytes[A_INDEX], get_flag(register_bit::CARRY_BIT));
		break;
	case 0xa0:
		op_and(m_registers.bytes[B_INDEX]);
		break;
	case 0xa1:
		op_and(m_registers.bytes[C_INDEX]);
		break;
	case 0xa2:
		op_and(m_registers.bytes[D_INDEX]);
		break;
	case 0xa3:
		op_and(m_registers.bytes[E_INDEX]);
		break;
	case 0xa4:
		op_and(m_registers.bytes[F_INDEX]);
		break;
	case 0xa5:
		op_and(m_registers.bytes[L_INDEX]);
		break;
	case 0xa6:
		op_and(z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0xa7:
		op_and(m_registers.bytes[A_INDEX]);
		break;
	case 0xa8:
		op_xor(m_registers.bytes[B_INDEX]);
		break;
	case 0xa9:
		op_xor(m_registers.bytes[C_INDEX]);
		break;
	case 0xaa:
		op_xor(m_registers.bytes[D_INDEX]);
		break;
	case 0xab:
		op_xor(m_registers.bytes[E_INDEX]);
		break;
	case 0xac:
		op_xor(m_registers.bytes[F_INDEX]);
		break;
	case 0xad:
		op_xor(m_registers.bytes[L_INDEX]);
		break;
	case 0xae:
		op_xor(z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0xaf:
		op_xor(m_registers.bytes[A_INDEX]);
		break;
	case 0xb0:
		op_or(m_registers.bytes[B_INDEX]);
		break;
	case 0xb1:
		op_or(m_registers.bytes[C_INDEX]);
		break;
	case 0xb2:
		op_or(m_registers.bytes[D_INDEX]);
		break;
	case 0xb3:
		op_or(m_registers.bytes[E_INDEX]);
		break;
	case 0xb4:
		op_or(m_registers.bytes[F_INDEX]);
		break;
	case 0xb5:
		op_or(m_registers.bytes[H_INDEX]);
		break;
	case 0xb6:
		op_or(z80_memory_read(m_registers.shorts[HL_INDEX]));
		break;
	case 0xb7:
		op_or(m_registers.bytes[A_INDEX]);
		break;
	case 0xc1:
		pop(BC_INDEX);
		break;
	case 0xc2:
	{
		if (!get_flag(register_bit::ZERO_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xc3:
	{
		uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
		m_pc = addr;
		break;
	}
	case 0xc5:
		push(BC_INDEX);
		break;
	case 0xc6:
		add(z80_memory_read(m_pc + 1), 0);
		break;
	case 0xca:
	{
		if (get_flag(register_bit::ZERO_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xce:
		add(z80_memory_read(m_pc + 1), get_flag(register_bit::CARRY_BIT));
		break;
	case 0xd1:
		pop(DE_INDEX);
		break;
	case 0xd2:
	{
		if (!get_flag(register_bit::CARRY_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xd5:
		push(DE_INDEX);
		break;
	case 0xd6:
		sub(z80_memory_read(m_pc + 1), 0);
		break;
	case 0xd8:
	{
		if (get_flag(register_bit::CARRY_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xde:
		sub(z80_memory_read(m_pc + 1), get_flag(register_bit::CARRY_BIT));
		break;
	case 0xd9:
	{
		uint16_t tmp = m_alternate_regs[BC_INDEX];
		m_alternate_regs[BC_INDEX] = m_registers.shorts[BC_INDEX];
		m_registers.shorts[BC_INDEX] = tmp;
		tmp = m_alternate_regs[DE_INDEX];
		m_alternate_regs[DE_INDEX] = m_registers.shorts[DE_INDEX];
		m_registers.shorts[DE_INDEX] = tmp;
		tmp = m_alternate_regs[HL_INDEX];
		m_alternate_regs[HL_INDEX] = m_registers.shorts[HL_INDEX];
		m_registers.shorts[HL_INDEX] = tmp;
		break;
	}
	case 0xe1:
		pop(HL_INDEX);
		break;
	case 0xe2:
	{
		if (!get_flag(register_bit::PARITY_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xe3:
	{
		uint8_t tmp_high = m_registers.bytes[H_INDEX];
		uint8_t tmp_low = m_registers.bytes[L_INDEX];
		m_registers.bytes[L_INDEX] = z80_memory_read(m_registers.shorts[SP_INDEX]);
		m_registers.bytes[H_INDEX] = z80_memory_read(m_registers.shorts[SP_INDEX]+1);
		z80_memory_write(m_registers.shorts[SP_INDEX], tmp_high << 8 | tmp_low);
		break;
	}
	case 0xe5:
		push(HL_INDEX);
		break;
	case 0xe6:
		op_and(z80_memory_read(m_pc + 1));
		break;
	case 0xea:
	{
		if (get_flag(register_bit::PARITY_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xeb:
	{
		uint16_t tmp = m_registers.shorts[HL_INDEX];
		m_registers.shorts[HL_INDEX] = m_registers.shorts[DE_INDEX];
		m_registers.shorts[DE_INDEX] = tmp;
		break;
	}
	case 0xee:
		op_and(z80_memory_read(m_pc + 1));
		break;
	case 0xf1:
		pop(AF_INDEX);
		break;
	case 0xf2:
	{
		if (!get_flag(register_bit::SIGN_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	case 0xf5:
		push(AF_INDEX);
		break;
	case 0xf6:
		op_or(z80_memory_read(m_pc + 1));
		break;
	case 0xfa:
	{
		if (get_flag(register_bit::SIGN_BIT)) {
			uint16_t addr = z80_memory_read(m_pc) | z80_memory_read(m_pc + 1) << 8;
			m_pc = addr;
		}
		break;
	}
	}

	return 0;

}

// run the z80 tests
void z80_do_tests()
{
}
