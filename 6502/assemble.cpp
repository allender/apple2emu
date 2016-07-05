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
 * source code for assembly of 6502
*/
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <ctype.h>

#include "cpu.h"
#include "assemble.h"
#include "utils/path_utils.h"

#define QUOTE(name)    #name
#define STR(name)      QUOTE(name)

#define MAX_LINE       1024     // maximum line length
#define MAX_LABEL      16       // maximum length of a label
#define COMMENT_CHAR   ';'

enum class record_type : uint8_t {
	RECORD_TYPE_INSTRUCTION,
	RECORD_TYPE_DIRECTIVE,
	RECORD_TYPE_COMMENT,
	RECORD_TYPE_ERROR
};

struct symbol_table_entry {
	uint16_t m_location;        // location of the symbol
};

std::unordered_map<std::string, symbol_table_entry> Symbol_table;

static uint16_t location_counter;		// 2 byte memory addressing
static uint8_t Parsed_opcode;

void add_label_to_symtable(const char *label) {
	if (label != nullptr) { // we have a label.  Grab it and put it into the symbol table
		symbol_table_entry entry;
		entry.m_location = location_counter;
		Symbol_table[label] = entry;
	}
}

static bool validate_label(const char *label)
{
	const char *p = label;
	while ((isalnum(*p) || (*p == ':')) && *p != '\0') {
		p++;
	}
	if (*p == '\0') {
		return true;
	}
	return false;
}

// returns true if the line is a comment, false otherwise
static bool is_comment(char *line)
{
	char *p = line;
	while (isspace(*p) && *p != '\0') {
		p++;
	}
	// empty line or we have found comment character
	if ( (*p == '\0') || (*p == ';') ) {
		return true;
	}
	return false;
}

// parse out the line into its individual components.  Assumes that the line
// is not a comment or empty line
static void get_line_components(char *line, const char *&label_str, const char *&opcode_str, const char *&addressing_str)
{
	static char internal_line[MAX_LINE];

	label_str = nullptr;
	opcode_str = nullptr;
	addressing_str = nullptr;

	strcpy(internal_line, line);
	char *p = internal_line;

	// labels must start at the beginning of the line
	if (isalnum(*p)) {
		label_str = p;
		while (isalnum(*p) || (*p == ':')) {
			p++;
		}
		if (*(p-1) == ':') {
			*(p-1) = '\0';
		}
		*p++ = '\0';
	}
	if (*p == '\0') {
		return;
	}

	// get the opcode
	while (isspace(*p)) {
		p++;
	}
	opcode_str = p;
	for ( auto i = 0; i < 3; i++ ) {
		toupper(*p++);
	}
	*p++ = '\0';

	// get the operand
	while (isspace(*p)) {
		*p++;
	}
	addressing_str = p;
	while ((*p != '\n') && (*p != ';')) {
		p++;
	}

	// null terminate if at comment
	if (*p == ';') {
		p--;
		while (isspace(*p)) {
			p--;
		}
	}
	*p = '\0';
}

static record_type parse_line(char *line)
{
	uint32_t opcode;

	Parsed_opcode = 0xff;
	if (is_comment(line)) {
		return record_type::RECORD_TYPE_COMMENT;
	};

	// check for label, which starts in the first column
	const char *label_str, *opcode_str, *addressing_str;
	get_line_components(line, label_str, opcode_str, addressing_str);
	if (label_str != nullptr) {
		if (validate_label(label_str)) {
			add_label_to_symtable(label_str);
		} else {
			// invalid label
		}
		if (opcode_str == nullptr) {
			return record_type::RECORD_TYPE_COMMENT;
		}
	}

	// life is easy when all opcodes are 3 characters
	_ASSERT(strlen(opcode_str) == 3);
	opcode = opcode_str[0] << 24 | opcode_str[1] << 16 | opcode_str[2] << 8 | ' ';

	cpu_6502::addressing_mode mode = cpu_6502::addressing_mode::NO_MODE;
	const char *p = &addressing_str[1];
	if (addressing_str[0] == '#') {
		// #$c0  -- Immediate
		mode = cpu_6502::addressing_mode::IMMEDIATE_MODE;
	} else if (addressing_str[0] == '(') {
		// indirect modes
		// ($c000) -- indirect
		// ($c0,X) -- Indexed indirect
		// ($c0),Y -- Indirect indexed
		while (*p != '\0') {
			if ((*p == 'X') || (*p == 'x')) {
				mode = cpu_6502::addressing_mode::INDEXED_INDIRECT_MODE;
				break;
			}
			if ((*p == 'Y') || (*p == 'y')) {
				mode = cpu_6502::addressing_mode::INDIRECT_INDEXED_MODE;
				break;
			}
			p++;
		}
		if (*p == '\0') {
			mode = cpu_6502::addressing_mode::INDIRECT_MODE;
		}
	} else if (addressing_str[0] == '$') {
		// $c000 -- Absolute
		// $c0   -- Zero Page
		// $c0   -- Relative (which is the same a zero page - need to be careful when picking the mode)
		// $c0,X -- Zero Page X
		// $c0,Y -- Zero Page Y (same size as above)
		// $c000,X -- Indexed X
		// $c000,Y -- Indexed Y

		// skip to comma or end of string
		while (isalnum(*p) && (*p != '\0')) {
			p++;
		}
		int size = p - addressing_str - 1;
		if (*p == '\0') {
			if (size == 2) {
				mode = cpu_6502::addressing_mode::ZERO_PAGE_MODE;
			} else if (size == 4) {
				mode = cpu_6502::addressing_mode::ABSOLUTE_MODE;
			}
		} else {
			// we hit a comma.  SKip past that and eat white space
			p++;
			while (isspace(*p) && (*p != '\0')) {
				p++;
			}
			if ((*p == 'x') || (*p == 'X') ) {
				if (size == 2) {
				} else if (size == 4) {
					mode = cpu_6502::addressing_mode::X_INDEXED_MODE;
				}
			} else if ((*p == 'y') || (*p == 'Y') ) { 
				if (size == 4) {
					mode = cpu_6502::addressing_mode::Y_INDEXED_MODE;
				}
			}
		}
	} else {
		// this is a label.  Validate the label.  don't add to symbol table as
		// that only need to be done when label is at specific address, not
		// referencing an address
		if (validate_label(addressing_str) == false) {
			// invalid label
		}
	}

	// find the addressing mode and opcode in the list and get the number of
	// bytes needed
	for (auto i = 0; i < 256; i++ ) {
		if (cpu_6502::m_opcodes[i].m_mnemonic == opcode) {
			if ((mode == cpu_6502::addressing_mode::NO_MODE) ||
				 (cpu_6502::m_opcodes[i].m_addressing_mode == mode) ||
				 ((cpu_6502::m_opcodes[i].m_addressing_mode == cpu_6502::addressing_mode::RELATIVE_MODE) && (mode == cpu_6502::addressing_mode::ZERO_PAGE_MODE))) {
				Parsed_opcode = i;
				break;
			}
		}
	}
	_ASSERT(Parsed_opcode != 0xff);

	return record_type::RECORD_TYPE_INSTRUCTION;
}

// gets a byte from the current string.  
static uint8_t encode_byte(const char *p)
{
	if (*p == '$') {
		p++;
	}
	return static_cast<uint8_t>(strtol(p, nullptr, 16));
}

static uint16_t encode_word(const char *p)
{
	if (*p == '$') {
		p++;
	}
	uint16_t word = static_cast<uint16_t>(strtol(p, nullptr, 16));

	// reverse the order of the words in big endian mode.  In little endian mode
	// the word will be stored in the correct order
#if defined(LITTLE_ENDIAN)
	return word;
#else
	return ((word & 0xff) << 8 | (word & 0xff00) >> 8);
#endif // defined(LITTLE_ENDIAN)
}

static uint16_t encode_word_or_label(const char *p)
{
	if (*p == '$') {
		return static_cast<uint16_t>(strtol(p+1, nullptr, 16));
	}

	if (validate_label(p) == true) {
		auto entry = Symbol_table.find(p);
		if (entry != Symbol_table.end()) {
			return static_cast<uint16_t>(entry->second.m_location);
		} else {
			// unable to find label in symbol table
		}
	} else {
		// unable to find label
	}
	return 0;
}

// relative addressing mode - could be label or value
static uint8_t encode_relative_address(const char *p, uint16_t pc)
{
	if (*p == '$') {
		return static_cast<uint8_t>(strtol(p+1, nullptr, 16));
	}

	if (validate_label(p) == true) {
		auto entry = Symbol_table.find(p);
		if (entry != Symbol_table.end()) {
			uint16_t diff = entry->second.m_location - (pc + 2);
			if ((pc > SCHAR_MAX) || (pc < SCHAR_MIN)) {
				// out of bounds
			} else {
				return static_cast<uint8_t>(diff);
			}
		} else {
			// unable to find label in symbol table
		}
	} else {
		// unable to find label
	}
	return 0;
}

static uint8_t encode_instruction(uint8_t *buffer, uint8_t opcode, char *source_line, uint16_t pc)
{
	uint8_t loc = 0;
	uint16_t addr;
	buffer[loc++] = opcode;
	const char *addressing_loc = &source_line[4];

	uint8_t size = cpu_6502::m_opcodes[opcode].m_size;
	cpu_6502::addressing_mode addr_mode = cpu_6502::m_opcodes[opcode].m_addressing_mode;

	// parse out the source line
	const char *label_str, *opcode_str, *addressing_str;
	get_line_components(source_line, label_str, opcode_str, addressing_str);

	switch(addr_mode) {
	case cpu_6502::addressing_mode::IMMEDIATE_MODE:
	case cpu_6502::addressing_mode::ZERO_PAGE_MODE:
		buffer[loc++] = encode_byte(addressing_str+1);
		break;

	case cpu_6502::addressing_mode::RELATIVE_MODE:
		// for branch statements
		buffer[loc++] = encode_relative_address(addressing_str, pc);
		break;

	case cpu_6502::addressing_mode::ACCUMULATOR_MODE:
		// just the opcode is needed here
		break;

	case cpu_6502::addressing_mode::IMPLIED_MODE:
		// just the opcode is required here
		break;

	case cpu_6502::addressing_mode::ABSOLUTE_MODE:
	case cpu_6502::addressing_mode::X_INDEXED_MODE:
	case cpu_6502::addressing_mode::Y_INDEXED_MODE:
		addr = encode_word_or_label(addressing_str);
		memcpy(&buffer[loc], &addr, sizeof(addr));
		loc += 2;
		break;

	case cpu_6502::addressing_mode::INDIRECT_MODE:
		addr = encode_word(addressing_str+1);
		memcpy(&buffer[loc], &addr, sizeof(addr));
		loc += 2;
		break;
	case cpu_6502::addressing_mode::ZERO_PAGE_INDEXED_MODE:
		buffer[loc++] = encode_byte(addressing_str);
		break;
	case cpu_6502::addressing_mode::INDEXED_INDIRECT_MODE:
	case cpu_6502::addressing_mode::INDIRECT_INDEXED_MODE:
		buffer[loc++] = encode_byte(addressing_str+1);
		break;
	default:
		break;
	}

	return loc;
}

bool assemble_6502(const std::string &input_filename)
{
	std::string output_filename;
	std::string int_filename;

	// create an output filename based on the input filename
	path_utils_change_ext(output_filename, input_filename, ".bin");
	path_utils_change_ext(int_filename, input_filename, ".tmp");

	FILE *input_fp = fopen(input_filename.c_str(), "rt");
	if (input_fp == nullptr) {
		printf("Unable to open %s for reading\n", input_filename);
		return false;
	}


	// two pass implementation.  First pass will send information to // temporary file
	FILE *intermediate_fp = fopen(int_filename.c_str(), "wt");
	if (intermediate_fp == nullptr) {
		printf("Unable to open intermediate file %s for writing\n", int_filename);
		return false;
	}

	location_counter = 0;

	// PASS 1
	char line[MAX_LINE];
	while (fgets(line, MAX_LINE, input_fp) != nullptr) {
		// parse line - will set global when the line contants instructions
		record_type rtype = parse_line(line);
		uint16_t bytes_needed = 0;
		if (rtype == record_type::RECORD_TYPE_INSTRUCTION) {
			bytes_needed = cpu_6502::m_opcodes[Parsed_opcode].m_size;
		}

		fprintf(intermediate_fp, "%d\t0x%04x\t0x%02x\t%s", rtype, location_counter, Parsed_opcode, line);
		location_counter += bytes_needed;
	}
	fclose(input_fp);

	// start pass 2
	fclose(intermediate_fp);
	intermediate_fp = fopen(int_filename.c_str(), "rt");

	FILE *output_fp = fopen(output_filename.c_str(), "wb");
	if (output_fp == nullptr) {
		printf("Unable to open %s for writing\n", output_filename);
		return false;
	}

	// iterate over intermediate file and put together the assembled file
	uint16_t pc = 0;
	while (fgets(line, MAX_LINE, intermediate_fp) != nullptr) {
		char *record_string = strtok(line, "\t");
		record_type rtype = static_cast<record_type>(atoi(record_string));
		if (rtype == record_type::RECORD_TYPE_INSTRUCTION) {
			location_counter = static_cast<uint16_t>(strtol(strtok(nullptr, "\t"), nullptr, 16));
			uint8_t opcode = static_cast<uint8_t>(strtol(strtok(nullptr, "\t"), nullptr, 16));
			char *source_line = strtok(nullptr, "\n");

			// encode the opcode to a buffer and then write it out
			uint8_t buffer[256];
			uint8_t buffer_size = encode_instruction(buffer, opcode, source_line, pc);
			if (buffer_size > 0) {
				fwrite(&buffer, sizeof(uint8_t), buffer_size, output_fp);
			}

			// increment program counter by the number of bytes written to the file
			pc += buffer_size;
		}
	}
	
	// close files and done
	fclose(intermediate_fp);
	fclose(output_fp);
	
	return true;
}