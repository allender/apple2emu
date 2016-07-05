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

#if !defined(HEADER_6502)
#define HEADER_6502 

#include <functional>
#include <stdint.h>


class cpu_6502 {
public:
	enum class register_bit : uint8_t {
		CARRY_BIT = 0,
		ZERO_BIT,
		INTERRUPT_BIT,
		DECIMAL_BIT,
		BREAK_BIT,
		NOT_USED_BIT,
		OVERFLOW_BIT,
		SIGN_BIT,
	};

	enum class addressing_mode : uint8_t {
		NO_MODE = 0,
		ACCUMULATOR_MODE,
		IMMEDIATE_MODE,
		IMPLIED_MODE,
		RELATIVE_MODE,
		ABSOLUTE_MODE,
		ZERO_PAGE_MODE,
		INDIRECT_MODE,
		X_INDEXED_MODE,
		Y_INDEXED_MODE,
		ZERO_PAGE_INDEXED_MODE,
		ZERO_PAGE_INDEXED_MODE_Y,
		INDEXED_INDIRECT_MODE,
		INDIRECT_INDEXED_MODE,
		NUM_ADDRESSING_MODES
	};

	typedef int16_t (cpu_6502::*addressing_function)(void);

	typedef struct {
		uint32_t          m_mnemonic;
		uint8_t           m_size;
		uint8_t           m_cycle_count;
		addressing_mode   m_addressing_mode;
	} opcode_info;

	/*
	*  Table for all opcodes and their relevant data
	*/
	static opcode_info m_opcodes[256];
	
public:
	cpu_6502() { }
	void init();
	uint32_t process_opcode();
	void set_pc(uint16_t pc) { m_pc = pc; }

	// needed for debugger
	uint16_t get_pc()     { return m_pc; }
	uint8_t  get_acc()    { return m_acc;  }
	uint8_t  get_x()      { return m_xindex;  }
	uint8_t  get_y()      { return m_yindex;  }
	uint8_t  get_sp()     { return m_sp;  }
	uint8_t  get_status() { return m_status_register;  }

private:
	uint16_t         m_pc;
	uint8_t          m_sp;
	uint8_t          m_acc;
	uint8_t          m_xindex;
	uint8_t          m_yindex;
	uint8_t          m_status_register;

	addressing_function m_addressing_functions[static_cast<uint8_t>(addressing_mode::NUM_ADDRESSING_MODES)];

	void set_flag(register_bit bit, uint8_t val) { m_status_register = (m_status_register & ~(1<<static_cast<uint8_t>(bit))) | (!!val<<static_cast<uint8_t>(bit)); }
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
	int16_t zero_page_indexed_mode();
	int16_t zero_page_indexed_mode_y();
	int16_t indexed_indirect_mode();
	int16_t indirect_indexed_mode();

	void branch_relative();
};


#endif
