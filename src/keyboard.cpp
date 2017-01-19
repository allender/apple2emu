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

#include "SDL.h"

#include "apple2emu_defs.h"
#include "apple2emu.h"
#include "keyboard.h"
#include "video.h"
#include "debugger.h"
#include "interface.h"

#define KEY_SHIFT   (1<<8)
#define KEY_CTRL    (1<<9)

static bool Keyboard_caps_lock_on = true;

static const int Keybuffer_size = 32;

static int key_buffer_front, key_buffer_end;
static int key_buffer[Keybuffer_size];

// SDL scancode to ascii values
static uint8_t key_ascii_table[256] =
{ 0,   0,   0,   0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',   // 0x00 - 0x0f
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
static uint8_t key_shifted_ascii_table[256] =
{ 0,   0,   0,   0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',   // 0x00 - 0x0f
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

static uint8_t keyboard_get_key()
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
		key &= 0xff;
		key = key_shifted_ascii_table[key];
	}
	else if (key & KEY_CTRL) {
		// send ctrl-A through ctrl-Z
		key &= 0xff;
		if ((key >= SDL_SCANCODE_A) && (key <= SDL_SCANCODE_Z)) {
			key = key - SDL_SCANCODE_A + 1;
		}
		else {
			key = 0;
		}
	}
	else if (key == SDL_SCANCODE_RETURN) {
		key = '\r';
	}
	else if (key == SDL_SCANCODE_BACKSPACE) {
		key = '\b';
	}
	else if (key == SDL_SCANCODE_ESCAPE) {
		key = 0x9b;
	}
	else if (key == SDL_SCANCODE_RIGHT) {
		key = 0x95;
	}
	else if (key == SDL_SCANCODE_LEFT) {
		key = 0x88;
	}
	else {
		key = key_ascii_table[key];
		if (key >= 'a' && key <= 'z') {
			if (Emulator_type < emulator_type::APPLE2E || Keyboard_caps_lock_on) {
				key -= 32;
			}
		}
	}

	ASSERT(key <= UINT8_MAX);
	return static_cast<uint8_t>(key);
}

void keyboard_init()
{
	key_buffer_front = 0;
	key_buffer_end = 0;
	for (auto i = 0; i < Keybuffer_size; i++) {
		key_buffer[i] = 0;
	}
	if (Emulator_type >= emulator_type::APPLE2E) {
		Keyboard_caps_lock_on = true;
	} else {
		Keyboard_caps_lock_on = false;
	}
}

void keyboard_shutdown()
{
}

// process key event (could be key up or key down event)
// We will put key downs into the keyboard buffer
void keyboard_handle_event(SDL_Event &evt)
{
	// for now, no key repeats
	if (evt.key.repeat) {
		return;
	}

	if (evt.key.type == SDL_KEYDOWN) {
		uint32_t scancode = evt.key.keysym.scancode;
		SDL_Keymod mods = SDL_GetModState();

		// check for debugger
		if (scancode == SDL_SCANCODE_F12) {
			debugger_enter();
		}

		// maybe bring up the main menu
		if (scancode == SDL_SCANCODE_F1) {
			ui_toggle_main_menu();
		}
		if (scancode == SDL_SCANCODE_F2) {
			ui_toggle_debug_menu();
		}

		// if the caps lock key was down, change toggle internal
		// caps lock setting (for non apple2 machines)
		if (scancode == SDL_SCANCODE_CAPSLOCK) {
			Keyboard_caps_lock_on = !Keyboard_caps_lock_on;
		}


		if (scancode == SDL_SCANCODE_F9) {
			extern bool Debug_show_bitmap;
			Debug_show_bitmap = !Debug_show_bitmap;
		}

		// we should only be putting keys into the keyboard buffer that are printable
		// (i.e. in the apple ][ character set), or control keys.
		if (scancode < 256) {
			if (scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL ||
				scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT ||
				scancode == SDL_SCANCODE_CAPSLOCK ) {
				return;
			}
			if (mods & KMOD_SHIFT) {
				scancode |= KEY_SHIFT;
			}
			else if (mods & KMOD_CTRL) {
				scancode |= KEY_CTRL;
			}
			keyboard_insert_key(scancode);
		}
	}
}

static uint8_t last_key = 0;
uint8_t keyboard_read()
{
	uint8_t temp_key = keyboard_get_key();
	if (temp_key > 0) {
		last_key = temp_key | 0x80;
	}

	return(last_key);
}

uint8_t keyboard_clear()
{
	last_key &= 0x7F;
	return last_key;
}
