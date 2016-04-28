#include "SDL.h"
#include "keyboard.h"

#define KEY_SHIFT   (1<<8)
#define KEY_CTRL    (1<<9)

const uint32_t Keybuffer_size = 32;

uint32_t key_buffer_front, key_buffer_end;
uint32_t key_buffer[Keybuffer_size];

// SDL scancode to ascii values
uint8_t key_ascii_table[256] =
{  0,   0,   0,   0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',   // 0x00 - 0x0f
 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '1', '2',   // 0x10 - 0x1f
 '3', '4', '5', '6', '7', '8', '9', '0',   0,   0,   0,   0, ' ', '-', '=', '[',   // 0x20 - 0x2f
 ']', '\\',  0, ';', '\'', '`', ',', '.', '/',  0,   0,   0,   0,   0,   0,   0,   // 0x30 - 0x3f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x40 - 0x4f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x50 - 0x5f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x60 - 0x6f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x70 - 0x7f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x80 - 0x8f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x90 - 0x9f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xa0 - 0xaf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xb0 - 0xbf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xc0 - 0xcf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xd0 - 0xdf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xe0 - 0xef
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xf0 - 0xff
};


// SDL scancode to shifted ascii values
uint8_t key_shifted_ascii_table[256] =
{  0,   0,   0,   0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',   // 0x00 - 0x0f
 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',   // 0x10 - 0x1f
 '#', '$', '%', '^', '&', '*', '(', ')',   0,   0,   0,   0, ' ', '_', '+', '{',   // 0x20 - 0x2f
 '}', '|',   0, ':', '\"', '~', '<', '>', '?',  0,   0,   0,   0,   0,   0,   0,   // 0x30 - 0x3f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x40 - 0x4f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x50 - 0x5f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x60 - 0x6f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x70 - 0x7f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x80 - 0x8f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0x90 - 0x9f
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xa0 - 0xaf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xb0 - 0xbf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xc0 - 0xcf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xd0 - 0xdf
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xe0 - 0xef
	0,   0,   0,   0,    0,   0,   0,   0,   0,  0,   0,   0,   0,   0,   0,   0,   // 0xf0 - 0xff
};


// inserts a key into the keyboard buffer 
static void keyboard_insert_key(uint32_t code)
{
	// Buffer full, ignore keystroke
	if ((key_buffer_end + 1) % Keybuffer_size == key_buffer_front) {
		return;
	}

	key_buffer[key_buffer_end] = code;
	key_buffer_end = (key_buffer_end + 1) % Keybuffer_size;
}

uint32_t keyboard_get_key()
{
	uint32_t key;

	// Buffer empty
	if (key_buffer_front == key_buffer_end) {
		return 0;
	}

	key = key_buffer[key_buffer_front];

	do {
		key_buffer_front = (key_buffer_front + 1) % Keybuffer_size;
		// Advance past invalidated entries
	} while (key_buffer[key_buffer_front] == 0 && key_buffer_front != key_buffer_end);

	if (key & KEY_SHIFT) {
		key = key_shifted_ascii_table[key];
	} else if (key & KEY_CTRL) {
		// send ctrl-A through ctrl-Z
		key &= 0xff;
		if ((key >= SDL_SCANCODE_A) && (key <= SDL_SCANCODE_Z)) {
			key = key - SDL_SCANCODE_A + 1;
		} else {
			key = 0;
		}
	} else if (key == SDL_SCANCODE_RETURN) {
		key = '\r';
	} else if (key == SDL_SCANCODE_BACKSPACE) {
		key = '\b';
	} else {
		key = key_ascii_table[key];
	}
	return key;
}

void keyboard_init()
{
	key_buffer_front = 0;
	key_buffer_end = 0;
	for (auto i = 0; i < Keybuffer_size; i++) {
		key_buffer[i] = 0;
	}
}

void keyboard_shutdown()
{
}

// process key event (could be key up or key down event)
// We will put key downs into the keyboard buffer
void keyboard_handle_event(SDL_Event &evt)
{
	if (evt.key.type == SDL_KEYDOWN) {
		uint32_t scancode = evt.key.keysym.scancode;

		// we should only be putting keys into the keyboard buffer that are printable
		// (i.e. in the apple ][ character set), or control keys.
		if (scancode < 256) {
			if (scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL || scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
				return;
			}
			SDL_Keymod mods = SDL_GetModState();
			if (mods & KMOD_SHIFT) {
				scancode |= KEY_SHIFT;
			} else if (mods & KMOD_CTRL) {
				scancode |= KEY_CTRL;
			}
			keyboard_insert_key(scancode);
		}
	}
}
