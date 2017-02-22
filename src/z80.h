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

#pragma once

#include <stdint.h>

class z80 {

public:
	enum class register_bit : uint8_t {
		CARRY_BIT = 0,
		SUBTRACTION_BIT,
		PARITY_BIT,
		NOT_USED_BIT,
		HALF_CARRY_BIT,
		NOT_USED_BIT_2,
		ZERO_BIT,
		SIGN_BIT,
	};

	enum class addr_mode : uint8_t {
		NONE = 0,
		IMMEDIATE,
		IMMEDIATE_EXTENDED,
		MODIFIED_PAGE_ZERO,
		EXTENDED,
		INDEXED,
		REGISTER,
		IMPLIED,
		REGISTER_INDIRECT,
		BIT,
		NUM_ADDRESSING_MODES
	};


	typedef int16_t(z80::*addr_func)(void);

	typedef struct {
		uint32_t    m_mnemonic;
		uint8_t     m_size;
		uint8_t     m_cycle_count;
		addr_mode   m_addr_mode;
		addr_func   m_addr_func;
	} opcode_info;

	int16_t no_mode();
	uint32_t process_opcode();

private:

	// values that can be used for access into the  registers.  8 bit CPU
	// registers are paried as follows:
	//
	// AF
	// BC
	// DE
	// HL
	//
	// use little endian storage, these values will appear:
	//
	// FACBEDLH
	//
	// in the byte array for the registers so that when short values are
	// accessed, they are in the proper endian order
	//
	// note that since we are scoped within the z80 cpu class, these
	// enums will not be strongly typed.  Makes things a tad easier
	// when writing the emulation code
	enum reg_8bit {
		A_INDEX = 1,
		F_INDEX = 0,
		B_INDEX = 3,
		C_INDEX = 2,
		D_INDEX = 5,
		E_INDEX = 4,
		H_INDEX = 7,
		L_INDEX = 6,
		XH_INDEX = 9,
		XL_INDEX = 8,
		YH_INDEX = 11,
		YL_INDEX = 10,
		SPH_INDEX = 13,
		SPL_INDEX = 12,
		NUM_8BIT_REGS = 14
	};

	// indicies into the 2 byte entries int the registers union
	enum reg_16bit {
		AF_INDEX = 0,
		BC_INDEX,
		DE_INDEX,
		HL_INDEX,
		X_INDEX,
		Y_INDEX,
		SP_INDEX,
		NUM_16BIT_REGS
	};

	// sanity check while developing!!!
	static_assert(reg_8bit::NUM_8BIT_REGS == (reg_16bit::NUM_16BIT_REGS * 2),
		"8 and 16 bit register counts don't match");

	// union for registers.
	union {
		uint8_t   bytes[reg_8bit::NUM_8BIT_REGS];
		uint16_t  shorts[reg_16bit::NUM_16BIT_REGS];
	} m_registers;

	// alternate register set.  We don't need direct access to these
	// registers. so we can just store them as shorts.  We just store
	// up to the HL index
	uint16_t         m_alternate_regs[reg_16bit::HL_INDEX];

	// interrupt and memory refresh registers
	uint8_t          m_i;
	uint8_t          m_r;

	uint8_t          m_flags;

	uint16_t         m_pc;

	static opcode_info m_opcodes[256];
	//opcode_info*     m_opcodes;   // these are the currently valid opcodes

	// internal functions to deal with register flags
	void set_flag(register_bit bit, uint8_t val) { m_flags = (m_flags & ~(1 << static_cast<uint8_t>(bit))) | (!!val << static_cast<uint8_t>(bit)); }
	uint8_t get_flag(register_bit bit) { return (m_flags >> static_cast<uint8_t>(bit)) & 0x1; }

	// helper functions to do the actual work
	void load_register(int reg, uint8_t val);
	void store_register(uint16_t addr, int reg);
	void store_immediate(uint16_t addr, uint8_t val);
	void pop(int reg);
	void push(int reg);
	void add(uint8_t val, int carry);
	void sub(uint8_t val, int carry);
	void and(uint8_t val);
	void xor(uint8_t val);
	void or(uint8_t val);
	void cp(uint8_t reg);
	void dec_register(int reg);
	void dec_indirect(uint16_t addr);
	void inc_register(int reg);
	void inc_indirect(uint16_t addr);


#if 0
private:
	/*
	*  Table for all opcodes and their relevant data
	*/
	static opcode_info m_65c02_opcodes[256];

public:
	cpu_6502() { }
	void init(cpu_6502::cpu_mode mode);
	void set_pc(uint16_t pc) { m_pc = pc; }

	// needed for debugger
	uint16_t get_pc() { return m_pc; }
	uint8_t  get_acc() { return m_acc; }
	uint8_t  get_x() { return m_xindex; }
	uint8_t  get_y() { return m_yindex; }
	uint8_t  get_sp() { return m_sp; }
	uint8_t  get_status() { return m_status_register; }

	cpu_6502::opcode_info *get_opcode(uint8_t opcode);

private:
	uint16_t         m_pc;
	uint8_t          m_sp;
	uint8_t          m_acc;
	uint8_t          m_xindex;
	uint8_t          m_yindex;
	uint8_t          m_status_register;
	uint8_t          m_extra_cycles;

	opcode_info*     m_opcodes;   // these are the currently valid opcodes

	void set_flag(register_bit bit, uint8_t val) { m_status_register = (m_status_register & ~(1 << static_cast<uint8_t>(bit))) | (!!val << static_cast<uint8_t>(bit)); }
	uint8_t get_flag(register_bit bit) { return (m_status_register >> static_cast<uint8_t>(bit)) & 0x1; }

	// addressing functions
	// non-indexed, non-memory
	int16_t accumulator_mode();
	int16_t immediate_mode();
	int16_t implied_mode();
	// non-indexed memory
	int16_t relative_mode();
	int16_t absolute_mode();
	int16_t zero_page_mode();
	int16_t indirect_mode();
	// indexed memory
	int16_t absolute_x_mode();
	int16_t absolute_y_mode();
	int16_t absolute_x_check_boundary_mode();
	int16_t absolute_y_check_boundary_mode();
	int16_t zero_page_indexed_mode();
	int16_t zero_page_indexed_mode_y();
	int16_t zero_page_indirect();
	int16_t indexed_indirect_mode();
	int16_t absolute_indexed_indirect_mode();
	int16_t indirect_indexed_mode();
	int16_t indirect_indexed_check_boundary_mode();

	void branch_relative();
#endif
};

void z80_do_tests();

