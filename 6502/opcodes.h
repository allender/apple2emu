#if !defined(OPCODES_H)
#define OPCODES_H

enum addressing_mode_t {
	NO_MODE  = -1,

	// non-index, non-memory
	ACC_MODE,
	IMM_MODE,
	IMP_MODE,

	// non indexed memory
	REL_MODE,
	ABS_MODE,
	ZPG_MODE,
	IND_MODE,

	// index memory
	ABX_MODE,
	ABY_MODE,
	ZPI_MODE,
	INX_MODE,
	INY_MODE,

	NUM_ADDRESSING_MODES
};

typedef struct {
	uint32_t          m_mnemonic;
	uint8_t           m_size;
	uint8_t           m_cycle_count;
	addressing_mode_t m_addressing_mode;
} opcode_info;

extern opcode_info Opcodes[256];

#endif // if !defined(OPCODES_H)