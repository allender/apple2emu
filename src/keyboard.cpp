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

#include "libclipboard.h"

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

static clipboard_c* Clipboard;
static const char *Clipboard_ptr;

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

	SDL_assert(key <= UINT8_MAX);

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

    Clipboard = clipboard_new(nullptr);
    Clipboard_ptr = nullptr;
}

void keyboard_shutdown()
{
}

// put key into the emulator buffer.  Code gets the key from the interface
// code which will send the key along when it isnt' used by some other
// UI element.  Code needs to make sure that we only queue keys that
// that the emulator needs
void keyboard_handle_event(uint32_t key, bool shift, bool ctrl, bool alt, bool super)
{
	// keys with shift, alt or super pushed can be ignored.  Shift can be
	// ignored because we will get they actual shifted key for output
	// in the key value itself (which comes from the imgui
	//
	if (shift == true || alt == true || super == true) {
		return;
	}

	// if control key is down, only allow control characters
	// from a-z
	if (ctrl == true && !(key >= 'a' && key <= 'z')) {
		return;
	}


	// if the caps lock key was down, change toggle internal
	// caps lock setting (for non apple2 machines)
	if (key == SDLK_CAPSLOCK) {
		Keyboard_caps_lock_on = !Keyboard_caps_lock_on;
	}

	// figure out the actual keyvalue to put onto the keyboard
	// buffer
	if (key == SDLK_RETURN) {
		key = '\r';
	}
	else if (key == SDLK_BACKSPACE) {
		key = '\b';
	}
	else if (key == SDLK_ESCAPE) {
		key = 0x1b;
	}
	else if (key == SDLK_RIGHT) {
		key = 0x15;
	}
	else if (key == SDLK_LEFT) {
		key = 0x08;
	}
	else if (ctrl == true) {
		// send ctrl-A through ctrl-Z
		if ((key >= SDLK_a) && (key <= SDLK_z)) {
			key = key - SDLK_a + 1;
		}
		else {
			key = 0;
		}
	} else if (key >= SDLK_a && key <= SDLK_z) {
		if (Emulator_type < emulator_type::APPLE2E || Keyboard_caps_lock_on) {
			key -= 32;
		}
	}

	// finally throw away any keys that aren't ASCII (which will get rid of
	// non-printable keys like function keys) that might get queued
	if (key > 127) {
		return;
	}

	keyboard_insert_key(key);
}

static uint8_t last_key = 0;

uint8_t keyboard_read()
{
    if (Clipboard_ptr != nullptr) {
        last_key = 0x80 | *Clipboard_ptr;
    } else {
        uint8_t temp_key = keyboard_get_key();
        if (temp_key > 0) {
            last_key = temp_key | 0x80;
        }
    }

	return last_key;
}

uint8_t keyboard_clear()
{
    if (Clipboard_ptr != nullptr) {
        last_key = 0x80 | *Clipboard_ptr;
        Clipboard_ptr++;
        if (*Clipboard_ptr == 0xa) {
            Clipboard_ptr++;
        }
        if (*Clipboard_ptr == '\0') {
            last_key = 0;
            Clipboard_ptr = nullptr;
        }
    } else {
        last_key &= 0x7F;
    }

	return last_key;
}

// get the clipboard and set the keyboard code to
// process the clipboard instead of keys
void keyboard_paste_clipboard()
{
    Clipboard_ptr = clipboard_text(Clipboard);
}
