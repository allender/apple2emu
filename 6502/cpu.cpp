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

/*
*  Source file for 6502 emulation layer
*/
#include <conio.h>
#include "cpu.h"
#include "string.h"

cpu_6502::opcode_info cpu_6502::m_opcodes[] = {
		// 0x00 - 0x0f
		{ 'BRK ', 1, 7, addressing_mode::IMPLIED_MODE }, { 'ORA ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE          }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE      }, { 'ORA ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'ASL ', 2, 0, addressing_mode::ZERO_PAGE_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'PHP ', 1, 3, addressing_mode::IMPLIED_MODE }, { 'ORA ', 2, 2, addressing_mode::IMMEDIATE_MODE        }, { 'ASL ', 1, 0, addressing_mode::ACCUMULATOR_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE      }, { 'ORA ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'ASL ', 3, 0, addressing_mode::ABSOLUTE_MODE    }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x10 - 
		{ 'BPL ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'ORA ', 2, 5, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'ORA ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'ASL ', 2, 0, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CLC ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'ORA ', 3, 4, addressing_mode::Y_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'ORA ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'ASL ', 3, 0, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x20 - 
		{ 'JSR ', 3, 6, addressing_mode::ABSOLUTE_MODE  }, { 'AND ', 2, 0, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE          }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'BIT ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { 'AND ', 2, 0, addressing_mode::ZERO_PAGE_MODE        }, { 'ROL ', 2, 5, addressing_mode::ZERO_PAGE_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'PLP ', 1, 4, addressing_mode::IMPLIED_MODE   }, { 'AND ', 2, 0, addressing_mode::IMMEDIATE_MODE        }, { 'ROL ', 1, 2, addressing_mode::ACCUMULATOR_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'BIT ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { 'AND ', 3, 0, addressing_mode::ABSOLUTE_MODE          }, { 'ROL ', 3, 6, addressing_mode::ABSOLUTE_MODE    }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x30 - 
		{ 'BMI ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'AND ', 2, 0, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'AND ', 2, 0, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'ROL ', 2, 6, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'SEC ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'AND ', 3, 0, addressing_mode::Y_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'AND ', 3, 0, addressing_mode::X_INDEXED_MODE         }, { 'ROL ', 3, 7, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x40 - 
		{ 'RTI ', 1, 6, addressing_mode::IMPLIED_MODE  }, { 'EOR ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE          }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'EOR ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'LSR ', 2, 5, addressing_mode::ZERO_PAGE_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'PHA ', 1, 3, addressing_mode::IMPLIED_MODE  }, { 'EOR ', 2, 2, addressing_mode::IMMEDIATE_MODE        }, { 'LSR ', 1, 2, addressing_mode::ACCUMULATOR_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'JMP ', 3, 3, addressing_mode::ABSOLUTE_MODE }, { 'EOR ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'LSR ', 3, 6, addressing_mode::ABSOLUTE_MODE    }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x50 - 
		{ 'BVC ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'EOR ', 2, 5, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'EOR ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'LSR ', 2, 6, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CLI ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'EOR ', 3, 4, addressing_mode::Y_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'EOR ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'LSR ', 3, 7, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x60 - 
		{ 'RTS ', 1, 6, addressing_mode::IMPLIED_MODE  }, { 'ADC ', 2, 0, addressing_mode::INDEXED_INDIRECT_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE          }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'ADC ', 2, 0, addressing_mode::ZERO_PAGE_MODE  }, { 'ROR ', 2, 5, addressing_mode::ZERO_PAGE_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'PLA ', 1, 4, addressing_mode::IMPLIED_MODE  }, { 'ADC ', 2, 0, addressing_mode::IMMEDIATE_MODE  }, { 'ROR ', 1, 2, addressing_mode::ACCUMULATOR_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'JMP ', 3, 5, addressing_mode::INDIRECT_MODE }, { 'ADC ', 0, 0, addressing_mode::ABSOLUTE_MODE  }, { 'ROR ', 3, 6, addressing_mode::ABSOLUTE_MODE    }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x70 - 
		{ 'BVS ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'ADC ', 3, 0, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'ADC ', 2, 0, addressing_mode::ZERO_PAGE_INDEXED_MODE  }, { 'ROR ', 2, 6, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'SEI ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'ADC ', 3, 0, addressing_mode::Y_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'ADC ', 3, 0, addressing_mode::X_INDEXED_MODE  }, { 'ROR ', 3, 7, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x80 - 
		{ '    ', 0, 0, addressing_mode::NO_MODE        }, { 'STA ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE        }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'STY ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { 'STA ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'STX ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'DEY ', 1, 2, addressing_mode::IMPLIED_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE               }, { 'TXA ', 1, 2, addressing_mode::IMPLIED_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'STY ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { 'STA ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'STX ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0x90 - 
		{ 'BCC ', 2, 0, addressing_mode::RELATIVE_MODE          }, { 'STA ', 2, 6, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'STY ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'STA ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'STX ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE_Y }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'TYA ', 1, 2, addressing_mode::IMPLIED_MODE           }, { 'STA ', 3, 5, addressing_mode::Y_INDEXED_MODE         }, { 'TXS ', 1, 2, addressing_mode::IMPLIED_MODE             }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE                }, { 'STA ', 3, 5, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xa0 - 
		{ 'LDY ', 2, 2, addressing_mode::IMMEDIATE_MODE }, { 'LDA ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { 'LDX ', 2, 2, addressing_mode::IMMEDIATE_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'LDY ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { 'LDA ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'LDX ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'TAY ', 1, 2, addressing_mode::IMPLIED_MODE   }, { 'LDA ', 2, 2, addressing_mode::IMMEDIATE_MODE        }, { 'TAX ', 1, 2, addressing_mode::IMPLIED_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'LDY ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { 'LDA ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'LDX ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xb0 - 
		{ 'BCS ', 2, 0, addressing_mode::RELATIVE_MODE          }, { 'LDA ', 2, 5, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'LDY ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'LDA ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'LDX ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE_Y }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CLV ', 1, 2, addressing_mode::IMPLIED_MODE           }, { 'LDA ', 3, 4, addressing_mode::Y_INDEXED_MODE         }, { 'TSX ', 1, 2, addressing_mode::IMPLIED_MODE             }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'LDY ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'LDA ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'LDX ', 3, 4, addressing_mode::Y_INDEXED_MODE           }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xc0 - 
		{ 'CPY ', 2, 2, addressing_mode::IMMEDIATE_MODE }, { 'CMP ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE        }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CPY ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { 'CMP ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'DEC ', 2, 5, addressing_mode::ZERO_PAGE_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'INY ', 1, 2, addressing_mode::IMPLIED_MODE   }, { 'CMP ', 2, 2, addressing_mode::IMMEDIATE_MODE        }, { 'DEX ', 1, 2, addressing_mode::IMPLIED_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CPY ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { 'CMP ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'DEC ', 3, 6, addressing_mode::ABSOLUTE_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xd0 - 
		{ 'BNE ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'CMP ', 2, 5, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'CMP ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'DEC ', 2, 6, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CLD ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'CMP ', 3, 4, addressing_mode::Y_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'CMP ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'DEC ', 3, 7, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xe0 - 
		{ 'CPX ', 2, 2, addressing_mode::IMMEDIATE_MODE }, { 'SBC ', 2, 6, addressing_mode::INDEXED_INDIRECT_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE        }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CPX ', 2, 3, addressing_mode::ZERO_PAGE_MODE }, { 'SBC ', 2, 3, addressing_mode::ZERO_PAGE_MODE        }, { 'INC ', 2, 5, addressing_mode::ZERO_PAGE_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'INX ', 1, 2, addressing_mode::IMPLIED_MODE   }, { 'SBC ', 2, 2, addressing_mode::IMMEDIATE_MODE        }, { 'NOP ', 1, 2, addressing_mode::IMPLIED_MODE   }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'CPX ', 3, 4, addressing_mode::ABSOLUTE_MODE  }, { 'SBC ', 3, 4, addressing_mode::ABSOLUTE_MODE         }, { 'INC ', 3, 6, addressing_mode::ABSOLUTE_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE },
		// 0xf0 - 
		{ 'BEQ ', 2, 0, addressing_mode::RELATIVE_MODE }, { 'SBC ', 2, 5, addressing_mode::INDIRECT_INDEXED_MODE  }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'SBC ', 2, 4, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { 'INC ', 2, 6, addressing_mode::ZERO_PAGE_INDEXED_MODE }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ 'SED ', 1, 2, addressing_mode::IMPLIED_MODE  }, { 'SBC ', 3, 4, addressing_mode::Y_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE                }, { '    ', 0, 0, addressing_mode::NO_MODE },
		{ '    ', 0, 0, addressing_mode::NO_MODE       }, { 'SBC ', 3, 4, addressing_mode::X_INDEXED_MODE         }, { 'INC ', 3, 7, addressing_mode::X_INDEXED_MODE         }, { '    ', 0, 0, addressing_mode::NO_MODE },
	};


//std::function<uint16_t(void)> addressing_mode_table[cpu_6502::addressing_mode::NUM_ADDRESSING_MODES] = {
//	[this] { return 0; },
//	[this] { return 0; },
//	[this] { int16_t val = m_pc++; return val; },
//	[this] { return 0; },
//	[this] { return 0; },
//	[this] { uint8_t lo = m_memory[m_pc++]; uint8_t hi = m_memory[m_pc++]; return (hi << 8) | lo; },
//	[this] { uint8_t lo = m_memory[m_pc++]; return lo; },
//	[this] { uint8_t lo = m_memory[m_pc++]; uint8_t hi = m_memory[m_pc++]; return (hi << 8) | lo; },
//	[this] { uint8_t lo = m_memory[m_pc++]; uint8_t hi = m_memory[m_pc++]; return ((hi << 8) | lo) + m_xindex; },
//	[this] { uint8_t lo = m_memory[m_pc++]; uint8_t hi = m_memory[m_pc++]; return ((hi << 8) | lo) + m_yindex; },
//	[this] { uint8_t lo = m_memory[m_pc++]; return lo + m_xindex; },
//	[this] { uint16_t addr_start = m_memory[m_pc++] + m_xindex; uint8_t lo = m_memory[addr_start]; uint8_t hi = m_memory[addr_start + 1]; return (hi << 8) | lo; },
//	[this] { uint8_t lo = m_memory[m_pc++]; uint8_t hi = m_memory[m_pc++]; uint16_t addr = ((hi << 8) | lo) + m_yindex; return addr; },
//};
uint16_t (cpu_6502::*mode_call)(void) = nullptr;



void cpu_6502::init()
{
	m_pc = 0;
	m_sp = 0xff;
	m_status_register = 0;
	set_flag(register_bit::NOT_USED_BIT, 1);

	// set up the addressing mode calls
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::NO_MODE)] = nullptr;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::ACCUMULATOR_MODE)] = &cpu_6502::accumulator_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::IMMEDIATE_MODE)] = &cpu_6502::immediate_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::IMPLIED_MODE)] = &cpu_6502::implied_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::RELATIVE_MODE)] = &cpu_6502::relative_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::ABSOLUTE_MODE)] = &cpu_6502::absolute_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::ZERO_PAGE_MODE)] = &cpu_6502::zero_page_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::INDIRECT_MODE)] = &cpu_6502::indirect_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::X_INDEXED_MODE)] = &cpu_6502::absolute_x_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::Y_INDEXED_MODE)] = &cpu_6502::absolute_y_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::ZERO_PAGE_INDEXED_MODE)] = &cpu_6502::zero_page_indexed_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::ZERO_PAGE_INDEXED_MODE_Y)] = &cpu_6502::zero_page_indexed_mode_y;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::INDEXED_INDIRECT_MODE)] = &cpu_6502::indexed_indirect_mode;
	m_addressing_functions[static_cast<uint8_t>(addressing_mode::INDIRECT_INDEXED_MODE)] = &cpu_6502::indirect_indexed_mode;
}

inline int16_t cpu_6502::accumulator_mode()
{
	return m_acc;
}

inline int16_t cpu_6502::immediate_mode()
{
	return m_pc++;
}

inline int16_t cpu_6502::implied_mode()
{
	return 0;  // this value won't get used
}

inline int16_t cpu_6502::relative_mode()
{
	return m_pc++;
}

inline int16_t cpu_6502::absolute_mode()
{
	uint8_t lo = m_memory[m_pc++];
	uint8_t hi = m_memory[m_pc++];
	return (hi << 8) | lo;
}

inline int16_t cpu_6502::zero_page_mode()
{
	uint8_t lo = m_memory[m_pc++];
	return lo;
}

inline int16_t cpu_6502::indirect_mode()
{
	uint8_t addr_lo = m_memory[m_pc++];
	uint8_t addr_hi = m_memory[m_pc++];
	uint16_t addr = addr_hi << 8 | addr_lo;
	uint16_t return_addr = m_memory[addr] & 0xff;
	return_addr |= (m_memory[addr+1] << 8);
	return return_addr;
}

inline int16_t cpu_6502::absolute_x_mode()
{
	uint8_t lo = m_memory[m_pc++];
	uint8_t hi = m_memory[m_pc++];
	return ((hi << 8) | lo) + m_xindex;
}

inline int16_t cpu_6502::absolute_y_mode()
{
	uint8_t lo = m_memory[m_pc++];
	uint8_t hi = m_memory[m_pc++];
	return ((hi << 8) | lo) + m_yindex;
}

inline int16_t cpu_6502::zero_page_indexed_mode()
{
	uint8_t lo = m_memory[m_pc++];
	return (lo + m_xindex) & 0xff;
}

inline int16_t cpu_6502::zero_page_indexed_mode_y()
{
	uint8_t lo = m_memory[m_pc++];
	return (lo + m_yindex) & 0xff;
}

inline int16_t cpu_6502::indexed_indirect_mode()
{
	uint16_t addr_start = (m_memory[m_pc++] + m_xindex) & 0xff;
	uint8_t lo = m_memory[addr_start];
	uint8_t hi = m_memory[addr_start + 1];
	return (hi << 8) | lo;
}

inline int16_t cpu_6502::indirect_indexed_mode()
{
	uint16_t addr_start = m_memory[m_pc++];
	uint8_t lo = m_memory[addr_start];
	uint8_t hi = m_memory[addr_start+1];
	uint16_t addr = ((hi << 8) | lo) + m_yindex;
	return addr;
}

inline void cpu_6502::branch_relative()
{
	uint8_t val = m_memory[m_pc-1];
	m_pc += val;
	if (val & 0x80) {
		m_pc -= 0x100;
	}
}

//
// main loop for opcode processing
//
uint32_t cpu_6502::process_opcode()
{
	// get the opcode at the program counter and the just figure out what
	// to do
	uint8_t opcode = m_memory[m_pc++];

	// get addressing mode and then do appropriate work based on the mode
	addressing_mode mode = m_opcodes[opcode].m_addressing_mode;
	_ASSERT( mode != addressing_mode::NO_MODE );
	uint16_t src = (this->*m_addressing_functions[static_cast<uint8_t>(mode)])();

	uint32_t cycles = m_opcodes[opcode].m_cycle_count;

	// execute the actual opcode
	switch(m_opcodes[opcode].m_mnemonic) {
	case 'ADC ':
		{
			uint8_t val = m_memory[src];
			uint32_t carry_bit = get_flag(register_bit::CARRY_BIT);
			uint32_t sum = (uint32_t)m_acc + (uint32_t)val + carry_bit;
			set_flag(register_bit::CARRY_BIT, sum > 0xff);
			set_flag(register_bit::OVERFLOW_BIT, ~(m_acc ^ val) & (m_acc ^ sum) & 0x80);
			if (get_flag(register_bit::DECIMAL_BIT)) {
				int32_t l = (m_acc & 0x0f) + (val & 0x0f) + carry_bit;
				int32_t h = (m_acc & 0xf0) + (val & 0xf0);
				if (l >= 0xa) {
					l = (l + 0x06) & 0xf;
					h += 0x10;
				}

				set_flag(register_bit::CARRY_BIT, 0);
				if (h >= 0xa0) {
					h += 0x60;
					set_flag(register_bit::CARRY_BIT, 1);
				}
				sum = h | l;
				//printf ("adc: %x + %x + %x = %x\n", m_acc, val, carry_bit, sum);
			}
			m_acc = sum & 0xff;
			set_flag(register_bit::ZERO_BIT, (m_acc == 0));
			set_flag(register_bit::SIGN_BIT, (m_acc & 0x80));
			break;
		}

	case 'AND ':
		{
			m_acc &= m_memory[src];
			set_flag(register_bit::SIGN_BIT, (m_acc>>7));
			set_flag(register_bit::ZERO_BIT, (m_acc==0));
			break;
		}

	case 'ASL ':
		{
			uint8_t val, old_val;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				old_val = m_acc;
			} else {
				old_val = m_memory[src];
			}
			val = old_val << 1;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				m_acc = val;
			} else {
				m_memory.write(src, val);
			}
			set_flag(register_bit::SIGN_BIT, val>>7);
			set_flag(register_bit::ZERO_BIT, val==0);
			set_flag(register_bit::CARRY_BIT, old_val>>7);
			break;
		}

	case 'BCC ':
		{
			if (get_flag(register_bit::CARRY_BIT) == 0) {
				branch_relative();
			}
			break;
		}
	case 'BCS ':
		{
			if (get_flag(register_bit::CARRY_BIT) == 1) {
				branch_relative();
			}
			break;
		}
	case 'BEQ ':
		{
			if (get_flag(register_bit::ZERO_BIT) == 1) {
				branch_relative();
			}
			break;
		}
	case 'BIT ':
		{
			int8_t val = m_memory[src];
			set_flag(register_bit::SIGN_BIT, ((val>>7)&0x1));
			set_flag(register_bit::OVERFLOW_BIT, ((val>>6)&0x1));
			set_flag(register_bit::ZERO_BIT, ((val & m_acc) == 0));
			break;
		}
	case 'BMI ':
		{
			if (get_flag(register_bit::SIGN_BIT) == 1) {
				branch_relative();
			}
			break;
		}
	case 'BNE ':
		{
			if (get_flag(register_bit::ZERO_BIT) == 0) {
				branch_relative();
			}
			break;
		}
	case 'BPL ':
		{
			if (get_flag(register_bit::SIGN_BIT) == 0) {
				branch_relative();
			}
			break;
		}
	case 'BRK ':
		{
			m_pc++;
			m_memory.write(0x100 + m_sp--, (m_pc >> 8));
			m_memory.write(0x100 + m_sp--, (m_pc & 0xff));
			uint8_t register_value = m_status_register;
			register_value |= (1<<static_cast<uint8_t>(register_bit::NOT_USED_BIT));
			register_value |= (1<<static_cast<uint8_t>(register_bit::BREAK_BIT));
			m_memory.write(0x100 + m_sp--, register_value);
			set_flag(register_bit::INTERRUPT_BIT, 1);
			m_pc = (m_memory[0xfffe] & 0xff) | (m_memory[0xffff] << 8);
			break;
		}
	case 'BVC ':
		{
			if (get_flag(register_bit::OVERFLOW_BIT) == 0) {
				branch_relative();
			}
			break;
		}
	case 'BVS ':
		{
			if (get_flag(register_bit::OVERFLOW_BIT) == 1) {
				branch_relative();
			}
			break;
		}
	case 'CLC ':
		{
			set_flag(register_bit::CARRY_BIT, 0);
			break;
		}
	case 'CLD ':
		{
			set_flag(register_bit::DECIMAL_BIT, 0);
			break;
		}
	case 'CLI ':
		{
			set_flag(register_bit::INTERRUPT_BIT, 0);
			break;
		}
	case 'CLV ':
		{
			set_flag(register_bit::OVERFLOW_BIT, 0);
			break;
		}
	case 'CMP ':
		{
			if (m_acc >= m_memory[src]) {
				set_flag(register_bit::CARRY_BIT, 1);
			} else {
				set_flag(register_bit::CARRY_BIT, 0);
			}
			int8_t val = m_acc - m_memory[src];
			set_flag(register_bit::SIGN_BIT, (val>>7)&0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
	case 'CPX ':
		{
			if (m_xindex >= m_memory[src]) {
				set_flag(register_bit::CARRY_BIT, 1);
			} else {
				set_flag(register_bit::CARRY_BIT, 0);
			}
			int8_t val = m_xindex - m_memory[src];
			set_flag(register_bit::SIGN_BIT, (val>>7)&0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
	case 'CPY ':
		{
			if (m_yindex >= m_memory[src]) {
				set_flag(register_bit::CARRY_BIT, 1);
			} else {
				set_flag(register_bit::CARRY_BIT, 0);
			}
			int8_t val = m_yindex - m_memory[src];
			set_flag(register_bit::SIGN_BIT, (val>>7)&0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
	case 'DEC ':
		{
			uint8_t val = m_memory[src] - 1;
			m_memory.write(src, val);
			set_flag(register_bit::SIGN_BIT, (val>>7)&0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
	case 'DEX ':
		{
			m_xindex--;
			set_flag(register_bit::SIGN_BIT, (m_xindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_xindex == 0) & 0x1);
			break;
		}
	case 'DEY ':
		{
			m_yindex--;
			set_flag(register_bit::SIGN_BIT, (m_yindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_yindex == 0) & 0x1);
			break;
		}
	case 'EOR ':
		{
			uint8_t val = m_memory[src];
			m_acc ^= val;
			set_flag(register_bit::SIGN_BIT, (m_acc >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_acc == 0) & 0x1);
			break;
		}
	case 'INC ':
		{
			uint8_t val = m_memory[src] + 1;
			m_memory.write(src, val);
			set_flag(register_bit::SIGN_BIT, (val>>7)&0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
		break;
	case 'INX ':
		{
			m_xindex++;
			set_flag(register_bit::SIGN_BIT, (m_xindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_xindex == 0) & 0x1);
			break;
		}
	case 'INY ':
		{
			m_yindex++;
			set_flag(register_bit::SIGN_BIT, (m_yindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_yindex == 0) & 0x1);
			break;
		}
	case 'JMP ':
		{
			m_pc = src;
			break;
		}
	case 'JSR ':
		{
#if defined(FUNCTIONAL_TESTS)
			if (src == 0xf001) {
				printf("%c", m_acc);
			} else if (src == 0xf004) {
				m_acc = _getch();
			} else {
				m_memory[0x100 + m_sp--] = ((m_pc - 1)>> 8);
				m_memory[0x100 + m_sp--] = (m_pc - 1) & 0xff;
				m_pc = src;
			}
#else
			m_memory.write(0x100 + m_sp--, ((m_pc - 1)>> 8));
			m_memory.write(0x100 + m_sp--, (m_pc - 1) & 0xff);
			m_pc = src;
#endif
			break;
		}
	case 'LDA ':
		{
			m_acc = m_memory[src];
			set_flag(register_bit::SIGN_BIT, (m_acc >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_acc == 0) & 0x1);
			break;
		}
	case 'LDX ':
		{
			m_xindex = m_memory[src];
			set_flag(register_bit::SIGN_BIT, (m_xindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_xindex == 0) & 0x1);
			break;
		}
	case 'LDY ':
		{
			m_yindex = m_memory[src];
			set_flag(register_bit::SIGN_BIT, (m_yindex >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, (m_yindex == 0) & 0x1);
			break;
		}
	case 'LSR ':
		{
		uint8_t val;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				val = m_acc;
			} else {
				val = m_memory[src];
			}
			set_flag(register_bit::CARRY_BIT, val & 1);
			val >>= 1;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				m_acc = val;
			} else {
				m_memory.write(src, val);
			}
			set_flag(register_bit::SIGN_BIT, 0);
			set_flag(register_bit::ZERO_BIT, val == 0);
			break;
		}
	case 'NOP ':
		{
			break;
		}
	case 'ORA ':
		{
			uint8_t val = m_memory[src];
			m_acc |= val;
			set_flag(register_bit::SIGN_BIT, (m_acc >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, m_acc == 0);
			break;
		}
	case 'PHA ':
		{
			m_memory.write(0x100 + m_sp--, m_acc);
			break;
		}
	case 'PLA ':
		{
			m_acc = m_memory[0x100 + ++m_sp];
			set_flag(register_bit::SIGN_BIT, (m_acc >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, m_acc == 0);
			break;
		}
	case 'PHP ':
		{
			uint8_t register_value = m_status_register;
			register_value |= 1 << (static_cast<uint8_t>(register_bit::BREAK_BIT));
			register_value |= 1 << (static_cast<uint8_t>(register_bit::NOT_USED_BIT));
			m_memory.write(0x100 + m_sp--, register_value);
			break;
		}
	case 'PLP ':
		{
			m_status_register = m_memory[0x100 + ++m_sp];
			break;
		}
	case 'ROL ':
		{
			uint8_t val;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				val = m_acc;
			} else {
				val = m_memory[src];
			}
			uint8_t carry_bit = (val >> 7) & 0x1;
			val <<= 1;
			val |= get_flag(register_bit::CARRY_BIT);
			set_flag(register_bit::CARRY_BIT, carry_bit);
			set_flag(register_bit::SIGN_BIT, (val >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				m_acc = val;
			} else {
				m_memory.write(src, val);
			}
			break;
		}
	case 'ROR ':
		{
			uint8_t val;
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				val = m_acc;
			} else {
				val = m_memory[src];
			}
			uint8_t carry_bit = val & 0x1;
			val >>= 1;
			val |= (get_flag(register_bit::CARRY_BIT) & 0x1) << 7;
			set_flag(register_bit::CARRY_BIT, carry_bit);
			set_flag(register_bit::SIGN_BIT, (val >> 7) & 0x1);
			set_flag(register_bit::ZERO_BIT, val == 0);
			if (mode == addressing_mode::ACCUMULATOR_MODE) {
				m_acc = val;
			} else {
				m_memory.write(src, val);
			}
			break;
		}
	case 'RTI ':
		{
			m_status_register = m_memory[0x100 + ++m_sp];
			m_pc = m_memory[0x100 + ++m_sp];
			m_pc = (m_memory[0x100 + ++m_sp] << 8) | m_pc;
			break;
		}
	case 'RTS ':
		{
			uint16_t addr = m_memory[0x100 + ++m_sp] & 0x00ff;
			addr |= (m_memory[0x100 + ++m_sp] << 8);
			m_pc = addr;
			m_pc++;
			break;
		}
	case 'SBC ':
		{
			uint8_t val = m_memory[src];
			uint32_t sum;
			uint32_t carry_bit = get_flag(register_bit::CARRY_BIT);
			sum = m_acc + (~val & 0xff) + carry_bit;
			set_flag(register_bit::CARRY_BIT, sum > 0xff);
			set_flag(register_bit::OVERFLOW_BIT, (m_acc ^ val) & (m_acc ^ sum) & 0x80);
			if (get_flag(register_bit::DECIMAL_BIT)) {
				// decimal mode.
				int32_t l = (m_acc & 0x0f) + (~val & 0x0f) + carry_bit;
				int32_t h = (m_acc & 0xf0) + (~val & 0xf0);
				if (l < 0x10) {
					l -= 0x06;
				} else {
					h += 0x10;
					l -= 0x10;
				}

				if (h < 0x100) {
					h -= 0x60;
					set_flag(register_bit::CARRY_BIT, 0);
				} else {
					set_flag(register_bit::CARRY_BIT, 1);
				}
				sum = h | l;
				//printf ("sbc: %x - %x + %x = %x\n", m_acc, val, carry_bit, sum);
			}
			m_acc = sum & 0xff;
			set_flag(register_bit::ZERO_BIT, (m_acc == 0));
			set_flag(register_bit::SIGN_BIT, (m_acc & 0x80));
			break;
		}
	case 'SEC ':
		{
			set_flag(register_bit::CARRY_BIT, 1);
			break;
		}
	case 'SED ':
		{
			set_flag(register_bit::DECIMAL_BIT, 1);
			break;
		}
	case 'SEI ':
		{
			set_flag(register_bit::INTERRUPT_BIT,1);
			break;
		}
	case 'STA ':
		{
			m_memory.write(src, m_acc);
			break;
		}
	case 'STX ':
		{
			m_memory.write(src, m_xindex);
			break;
		}
	case 'STY ':
		{
			m_memory.write(src, m_yindex);
			break;
		}
	case 'TAX ':
		{
			m_xindex = m_acc;
			set_flag(register_bit::ZERO_BIT, m_xindex == 0);
			set_flag(register_bit::SIGN_BIT, m_xindex & 0x80);
			break;
		}
	case 'TXA ':
		{
			m_acc = m_xindex;
			set_flag(register_bit::ZERO_BIT, m_acc == 0);
			set_flag(register_bit::SIGN_BIT, m_acc & 0x80);
			break;
		}
	case 'TAY ':
		{
			m_yindex = m_acc;
			set_flag(register_bit::ZERO_BIT, m_yindex == 0);
			set_flag(register_bit::SIGN_BIT, m_yindex & 0x80);
			break;
		}
	case 'TYA ':
		{
			m_acc = m_yindex;
			set_flag(register_bit::ZERO_BIT, m_acc == 0);
			set_flag(register_bit::SIGN_BIT, m_acc & 0x80);
			break;
		}
	case 'TXS ':
		{
			m_sp = m_xindex;
			break;
		}
	case 'TSX ':
		{
			m_xindex = (int8_t)m_sp;
			set_flag(register_bit::ZERO_BIT, m_xindex == 0);
			set_flag(register_bit::SIGN_BIT, m_xindex & 0x80);
			break;
		}

	default:
		{
			_ASSERT(0);
		}
		

	}

	// return the number of cycles that we executed
	return cycles;
}
