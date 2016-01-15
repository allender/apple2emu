/*
*  Source file for 6502 emulation layer
*/

#include "stdafx.h"

#include <stdint.h>

typedef void (*addressing_func)();

struct opcode_info {
	uint32_t          m_mnemonic;
	uint8_t           m_size;
	uint8_t           m_cycle_count;
	addressing_func   m_addressing_mode;
};

/*
* cpu stuff
*/
struct cpu_6502 {
	int16_t           m_pc;
	int16_t           m_sp;
	int16_t           m_acc;
	int16_t           m_xindex;
	int16_t           m_yindex;

	// status registers
	uint8_t           m_c_flag : 1;
	uint8_t           m_z_flag : 1;
	uint8_t           m_i_flag : 1;
	uint8_t           m_d_flag : 1;
	uint8_t           m_b_flag : 1;
	uint8_t           m_u_flag : 1;
	uint8_t           m_v_flag : 1;
	uint8_t           m_s_flag : 1;
};

/*
*  Table for all opcodes and their relevant data
*/

opcode_info Opcodes[] = 
{
	// 0x00 - 0x0f
	{ 'BRK ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { 'ASL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'PHP ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { 'ASL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { 'ASL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x10 - 
	{ 'BPL ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { 'ASL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CLC ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ORA ', 0, 0, nullptr }, { 'ASL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x20 - 
	{ 'JSR ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'BIT ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { 'ROL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'PLP ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { 'ROL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'BIT ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { 'ROL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x30 - 
	{ 'BMI ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { 'ROL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'SEC ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'AND ', 0, 0, nullptr }, { 'ROL ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x40 - 
	{ 'RTI ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { 'LSR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'PHA ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { 'LSR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'JMP ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { 'LSR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x50 - 
	{ 'BVC ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { 'LSR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CLI ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'EOR ', 0, 0, nullptr }, { 'LSR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x60 - 
	{ 'RTS ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { 'ROR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'PLA ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { 'ROR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'JMP ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { 'ROR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x70 - 
	{ 'BVS ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { 'ROR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'SEI ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'ADC ', 0, 0, nullptr }, { 'ROR ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x80 - 
	{ '    ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'STY ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { 'STX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'DEY ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { 'TXA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'STY ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { 'STX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0x90 - 
	{ 'BCC ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'STY ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { 'STX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'TYA ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { 'TXS ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'STA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xa0 - 
	{ 'LDY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'LDX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'LDY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'LDX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'TAY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'TAX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'LDY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xb0 - 
	{ 'BCS ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'LDY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'LDX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CLV ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'TSX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'LDY ', 0, 0, nullptr }, { 'LDA ', 0, 0, nullptr }, { 'LDX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xc0 - 
	{ 'CPY ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CPY ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { 'DEC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'INY ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { 'DEX ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CPY ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { 'DEC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xd0 - 
	{ 'BNE ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { 'DEC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CLD ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'CMP ', 0, 0, nullptr }, { 'DEC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xe0 - 
	{ 'CPX ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CPX ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { 'INC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'INX ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { 'NOP ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'CPX ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { 'INC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	// 0xf0 - 
	{ 'BEQ ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { 'INC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ 'SED ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
	{ '    ', 0, 0, nullptr }, { 'SBC ', 0, 0, nullptr }, { 'INC ', 0, 0, nullptr }, { '    ', 0, 0, nullptr },
};

void immediate_mode()
{
}

void absolute_mode()
{
}

void absolute_zero_page_mode()
{
}

void implied_mode()
{
}

void accumulator_mode()
{
}

void indexed_mode()
{
}

void indexed_zero_page_mode()
{
}

void indirect_mode()
{
}

void pre_indexed_indirect_mode()
{
}

void post_indexed_indirect_mode()
{
}

void relative_mode()
{
}

//
// main loop for opcode processing
//
void process_opcode(cpu_6502 *cpu)
{
	// get the opcode at the program counter and the just figure out what
	// to do
	uint8_t opcode = cpu->m_pc;
	switch(opcode) {
	case 'ADC ':
		break;
	case 'AND ':
		break;
	case 'ASL ':
		break;
	case 'BCC ':
		break;
	case 'BCS ':
		break;
	case 'BEQ ':
		break;
	case 'BIT ':
		break;
	case 'BMI ':
		break;
	case 'BNE ':
		break;
	case 'BPL ':
		break;
	case 'BRK ':
		break;
	case 'BVC ':
		break;
	case 'BVS ':
		break;
	case 'CLC ':
		break;
	case 'CLD ':
		break;
	case 'CLI ':
		break;
	case 'CLV ':
		break;
	case 'CMP ':
		break;
	case 'CPX ':
		break;
	case 'CPY ':
		break;
	case 'DEC ':
		break;
	case 'DEX ':
		break;
	case 'DEY ':
		break;
	case 'EOR ':
		break;
	case 'INC ':
		break;
	case 'INX ':
		break;
	case 'INY ':
		break;
	case 'JMP ':
		break;

	}
}