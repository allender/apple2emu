#include <stdio.h>
#include "memory.h"
#include "video.h"
#include "curses.h"
#include "debugger/debugger.h"

#pragma warning(disable:4996)   // disable the deprecated warnings for fopen

/*
 *  Screen display mapping table
 */

static char character_conv[] = {

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $00	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $08	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $10	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $18	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $20	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $28	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $30	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $38	*/

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $40	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $48	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $50	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $58	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $60	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $68	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $70	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $78	*/

	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g',		/* $80	*/
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',		/* $88	*/
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',		/* $90	*/
	'x', 'y', 'z', '[', '\\',']', '^', '_',		/* $98	*/
	' ', '!', '"', '#', '$', '%', '&', '\'',	   /* $A0	*/
	'(', ')', '*', '+', ',', '-', '.', '/',		/* $A8	*/
	'0', '1', '2', '3', '4', '5', '6', '7',		/* $B0	*/
	'8', '9', ':', ';', '<', '=', '>', '?',		/* $B8	*/

	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',		/* $C0	*/
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',		/* $C8	*/
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',		/* $D0	*/
	'X', 'Y', 'Z', '[', '\\',']', '^', '_',		/* $D8	*/
	' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g',		/* $E0	*/
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',		/* $E8	*/
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',		/* $F0	*/
	'x', 'y', 'z', ';', '<', '=', '>', '?',		/* $F8	*/

};

static bool key_clear = true;
static uint8_t last_key = 0;

uint8_t keyboard_read_handler(uint16_t addr)
{
	if (key_clear) {
		char c = getch();
		if ( c != ERR ) {
			uint8_t temp_key = c;
			
			// check to see if we hit the debugger key.  If so, don't
			// report this as key to the system
			if (temp_key == 26) {
				debugger_enter();
				return 0;
			}
			key_clear = false;
			if (temp_key == '\n')
				temp_key = '\r';
			else if (temp_key == 127)
				temp_key = 8;
			if ( temp_key == '~') {
				int x = 5;
			}
			last_key = temp_key | 0x80;
		}
	}
	return(last_key);
}

uint8_t keyboard_clear_handler(uint16_t addr)
{
	key_clear = true;
	last_key &= 0x7F;
	return last_key;
}

// memory constructor
memory::memory(int size, void *memory)
{
	m_size = size;
	m_memory = (uint8_t *)memory;

	// load up ROM/PROM images
	load_memory("apple2.rom", 0xd000);
	load_memory("disk2.rom", 0xc600);

	// set up screen map (temporary) for video output
	for (int i = 0; i < MAX_LINES; i++) {
		m_screen_map[i] = 1024 + 256 * ((i/2) % 4)+(128*(i%2))+40*((i/8)%4);
	}

	for (auto i = 0; i < 256; i++) {
		m_c000_handlers[i].m_read_handler = nullptr;
		m_c000_handlers[i].m_write_handler = nullptr;
	}

	// register handlers for keyboard -- temporary I think
	m_c000_handlers[0x00].m_read_handler = keyboard_read_handler;
	m_c000_handlers[0x10].m_read_handler = keyboard_clear_handler;
}

// register handlers for the I/O slots
bool memory::register_slot_handler(const uint8_t slot, slot_io_read_function read_function, slot_io_write_function write_function )
{
	_ASSERT((slot >= 1) && (slot <= MAX_SLOTS));
	uint8_t addr = 0x80 + (slot << 4);
	
	// add handlers for the slot.  There are 16 addresses per slot so set them all to the same 
	// handler as we will deal with all slot operations in the same function
	for (auto i = 0; i <= 0xf; i++) {
		m_c000_handlers[addr+i].m_read_handler = read_function;
		m_c000_handlers[addr+i].m_write_handler = write_function;
	}
	return true;
}

uint8_t memory::operator[](const uint16_t addr)
{
	static uint8_t last_key = 0;

	// look for memory mapped I/O locations
	if ((addr & 0xff00) == 0xc000) {
		uint8_t mapped_addr = addr & 0xff;
		if (m_c000_handlers[mapped_addr].m_read_handler != nullptr ) {
			return m_c000_handlers[mapped_addr].m_read_handler(addr);
		}
	}
	return m_memory[addr];
}

// function to write value to memory.  Trapped here in order to
// faciliate memory mapped I/O and other similar things
void memory::write(const uint16_t addr, uint8_t val)
{
	if (addr >= 0xd000) {
		return;
	}

	m_memory[addr] = val;

	// see if memory address is headed to video screen.  Text page 1
	if (((addr & 0x0f00) >= 0x400) && ((addr & 0x0f00) < 0x800)) {
		for ( int i = 0; i < MAX_LINES; i++ ) {
			if ((addr >= m_screen_map[i]) && (addr < (m_screen_map[i] + MAX_COLUMNS))) {
				move_cursor(i, addr - m_screen_map[i]);
				if (val <= 0x3f) {
					set_inverse();
				} else if (val <= 0x7f) {
					set_flashing();
				} else {
					set_normal();
				}
				output_char(character_conv[val]);
				break;
			}
		}
	}
}

bool memory::load_memory(const char *filename, uint16_t location)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0, SEEK_END);
	uint32_t buffer_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// allocate the memory buffer
	fread(&m_memory[location], 1, buffer_size, fp);
	fclose(fp);
	return true;
}

bool memory::load_memory(uint8_t *buffer, uint16_t size, uint16_t location)
{
	memcpy(&(m_memory[location]), buffer, size);
	return true;
}

