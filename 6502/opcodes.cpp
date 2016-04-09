// 
// main source file for opcodes for the 6502
//

#include "opcodes.h"
#include "cpu.h"    // gets the function pointers for the addressing modes


opcode_info Opcodes[] = {
		// 0x00 - 0x0f
		{ 'BRK ', 1, 7, IMP_MODE }, { 'ORA ', 2, 6, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ORA ', 2, 3, ZPG_MODE }, { 'ASL ', 2, 0, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'PHP ', 1, 3, IMP_MODE }, { 'ORA ', 2, 2, IMM_MODE }, { 'ASL ', 1, 0, ACC_MODE }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ORA ', 3, 4, ABS_MODE }, { 'ASL ', 3, 0, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x10 - 
		{ 'BPL ', 2, 0, REL_MODE }, { 'ORA ', 2, 5, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ORA ', 2, 4, ZPI_MODE }, { 'ASL ', 2, 0, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CLC ', 1, 2, IMP_MODE }, { 'ORA ', 3, 4, ABY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ORA ', 3, 4, ABX_MODE }, { 'ASL ', 3, 0, ABX_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x20 - 
		{ 'JSR ', 3, 6, ABS_MODE }, { 'AND ', 2, 0, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'BIT ', 2, 3, ZPG_MODE }, { 'AND ', 2, 0, ZPG_MODE }, { 'ROL ', 2, 5, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'PLP ', 1, 4, IMP_MODE }, { 'AND ', 2, 0, IMM_MODE }, { 'ROL ', 1, 2, ACC_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'BIT ', 3, 4, ABS_MODE }, { 'AND ', 3, 0, ABS_MODE }, { 'ROL ', 3, 6, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x30 - 
		{ 'BMI ', 2, 0, REL_MODE }, { 'AND ', 2, 0, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'AND ', 2, 0, ZPI_MODE }, { 'ROL ', 2, 6, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'SEC ', 1, 2, IMP_MODE }, { 'AND ', 3, 0, ABY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'AND ', 3, 0, ABX_MODE }, { 'ROL ', 3, 7, ABX_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x40 - 
		{ 'RTI ', 1, 6, IMP_MODE }, { 'EOR ', 2, 6, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'EOR ', 2, 3, ZPG_MODE }, { 'LSR ', 2, 5, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'PHA ', 1, 3, IMP_MODE }, { 'EOR ', 2, 2, IMM_MODE }, { 'LSR ', 1, 2, ACC_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'JMP ', 3, 3, ABS_MODE }, { 'EOR ', 3, 4, ABS_MODE }, { 'LSR ', 3, 6, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x50 - 
		{ 'BVC ', 2, 0, REL_MODE }, { 'EOR ', 2, 5, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'EOR ', 2, 4, ZPI_MODE }, { 'LSR ', 2, 6, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CLI ', 1, 2, IMP_MODE }, { 'EOR ', 3, 4, ABY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'EOR ', 3, 4, ABX_MODE }, { 'LSR ', 3, 7, ABX_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x60 - 
		{ 'RTS ', 1, 6, IMP_MODE }, { 'ADC ', 2, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ADC ', 2, 0, NO_MODE  }, { 'ROR ', 2, 5, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'PLA ', 1, 4, IMP_MODE }, { 'ADC ', 2, 0, NO_MODE  }, { 'ROR ', 1, 2, ACC_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'JMP ', 3, 5, IND_MODE }, { 'ADC ', 0, 0, NO_MODE  }, { 'ROR ', 3, 6, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x70 - 
		{ 'BVS ', 2, 0, REL_MODE }, { 'ADC ', 3, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ADC ', 2, 0, NO_MODE  }, { 'ROR ', 2, 6, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'SEI ', 1, 2, IMP_MODE }, { 'ADC ', 3, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'ADC ', 3, 0, NO_MODE  }, { 'ROR ', 3, 7, ABX_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x80 - 
		{ '    ', 0, 0, NO_MODE  }, { 'STA ', 2, 6, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'STY ', 2, 3, ZPG_MODE }, { 'STA ', 2, 3, ZPG_MODE }, { 'STX ', 2, 3, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'DEY ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE  }, { 'TXA ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'STY ', 3, 4, ABS_MODE }, { 'STA ', 3, 4, ABS_MODE }, { 'STX ', 3, 4, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0x90 - 
		{ 'BCC ', 2, 0, REL_MODE }, { 'STA ', 2, 6, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'STY ', 2, 4, ZPI_MODE }, { 'STA ', 2, 4, ZPI_MODE }, { 'STX ', 2, 4, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'TYA ', 1, 2, IMP_MODE }, { 'STA ', 3, 5, ABY_MODE }, { 'TXS ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'STA ', 3, 5, ABX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		// 0xa0 - 
		{ 'LDY ', 2, 2, IMM_MODE }, { 'LDA ', 2, 6, INX_MODE }, { 'LDX ', 2, 2, IMM_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'LDY ', 2, 3, ZPG_MODE }, { 'LDA ', 2, 3, ZPG_MODE }, { 'LDX ', 2, 3, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'TAY ', 1, 2, IMP_MODE }, { 'LDA ', 2, 2, IMM_MODE }, { 'TAX ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'LDY ', 3, 4, ABS_MODE }, { 'LDA ', 3, 4, ABS_MODE }, { 'LDX ', 3, 4, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0xb0 - 
		{ 'BCS ', 2, 0, REL_MODE }, { 'LDA ', 2, 5, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'LDY ', 2, 4, ZPI_MODE }, { 'LDA ', 2, 4, ZPI_MODE }, { 'LDX ', 2, 4, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CLV ', 1, 2, IMP_MODE }, { 'LDA ', 3, 4, ABY_MODE }, { 'TSX ', 1, 2, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'LDY ', 3, 4, ABX_MODE }, { 'LDA ', 3, 4, ABX_MODE }, { 'LDX ', 3, 4, ABY_MODE }, { '    ', 0, 0, NO_MODE },
		// 0xc0 - 
		{ 'CPY ', 2, 2, IMM_MODE }, { 'CMP ', 2, 6, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'CPY ', 2, 3, ZPG_MODE }, { 'CMP ', 2, 3, ZPG_MODE }, { 'DEC ', 2, 5, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'INY ', 1, 2, IMP_MODE }, { 'CMP ', 2, 2, IMM_MODE }, { 'DEX ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CPY ', 3, 4, ABS_MODE }, { 'CMP ', 3, 4, ABS_MODE }, { 'DEC ', 3, 6, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0xd0 - 
		{ 'BNE ', 2, 0, REL_MODE }, { 'CMP ', 2, 5, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'CMP ', 2, 4, ZPI_MODE }, { 'DEC ', 2, 6, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CLD ', 1, 2, IMP_MODE }, { 'CMP ', 3, 4, ABY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'CMP ', 3, 4, ABX_MODE }, { 'DEC ', 3, 7, ABX_MODE }, { '    ', 0, 0, NO_MODE },
		// 0xe0 - 
		{ 'CPX ', 2, 2, IMM_MODE }, { 'SBC ', 2, 6, INX_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ 'CPX ', 2, 3, ZPG_MODE }, { 'SBC ', 2, 3, ZPG_MODE }, { 'INC ', 2, 5, ZPG_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'INX ', 1, 2, IMP_MODE }, { 'SBC ', 2, 2, IMM_MODE }, { 'NOP ', 1, 2, IMP_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'CPX ', 3, 4, ABS_MODE }, { 'SBC ', 3, 4, ABS_MODE }, { 'INC ', 3, 6, ABS_MODE }, { '    ', 0, 0, NO_MODE },
		// 0xf0 - 
		{ 'BEQ ', 2, 0, REL_MODE }, { 'SBC ', 2, 5, INY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'SBC ', 2, 4, ZPI_MODE }, { 'INC ', 2, 6, ZPI_MODE }, { '    ', 0, 0, NO_MODE },
		{ 'SED ', 1, 2, IMP_MODE }, { 'SBC ', 3, 4, ABY_MODE }, { '    ', 0, 0, NO_MODE  }, { '    ', 0, 0, NO_MODE },
		{ '    ', 0, 0, NO_MODE  }, { 'SBC ', 3, 4, ABX_MODE }, { 'INC ', 3, 7, ABX_MODE }, { '    ', 0, 0, NO_MODE },
	};

