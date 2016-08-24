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

#include "SDL.h"
#include "apple2emu.h"
#include "joystick.h"

static int Num_controllers;
static const char *Game_controller_mapping_file = "gamecontrollerdb.txt";

// information for joysticks
class controller {
public:
	bool                     m_initialized;   // is this joystick initialized
	SDL_GameController      *m_gc;            // pointer to game controller structure
	const char              *m_name;          // name of controller
	int8_t                   m_button_state[SDL_CONTROLLER_BUTTON_MAX];
	int16_t                  m_axis_state[SDL_CONTROLLER_AXIS_MAX];
	uint32_t                 m_axis_timer_state[SDL_CONTROLLER_AXIS_MAX];
	SDL_GameControllerButton m_buttons[SDL_CONTROLLER_BUTTON_MAX];  // mapping from button number to button enum
	SDL_GameControllerAxis   m_axis[SDL_CONTROLLER_AXIS_MAX];       // mapping from axis number to axis enum

	controller() :m_initialized(false) {};

};

const float Joystick_cycles_scale = 2816.0f / 255.0f;   // ~2816 total cycles for 558 timer
const int Max_controllers = 16;
controller Controllers[Max_controllers];

// reads the status of button num from the joystick structures
static uint8_t joystick_read_button(uint8_t button_num)
{
	uint8_t val = SDL_GameControllerGetButton(Controllers[0].m_gc, Controllers[0].m_buttons[button_num]);
	// signals is active low.  So return 0 when on, and high bit set when not
	return val ? 0 : 0x80;
}

static uint8_t joystick_read_axis(uint8_t axis_num)
{
	// timer state for paddles is controlled by ths number of cycles.  The axis
	// timer state will have been previously set to the total cycles run
	// plus the axis value * number of cycles
	if (Total_cycles < Controllers[0].m_axis_timer_state[axis_num]) {
		return 0x80;
	}
	return 0;
}

uint8_t joystick_read_handler(uint16_t addr)
{
	uint8_t val = 0;

	addr = addr & 0xff;
	switch (addr) {
	case 0x61:
	case 0x62:
	case 0x63:
		val = joystick_read_button(addr - 0x61);
		break;
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
		val = joystick_read_axis(addr - 0x64);
		break;
	}

	return val;
}

void joystick_write_handler(uint16_t addr, uint8_t val)
{
	joystick_read_handler(addr);
}

// this function initiates the analog to digital conversion from the
// controllers (paddles/joysticks) to 0-255 digital value which will
// be read from the axis read handler.  Basically set up
// internal timer to indicate when the 558 timer should time out
// based on the paddle/joystick value.  Read Inside the Apple ][e
// chapter 10 for more information
uint8_t joystick_reset_read_handler(uint16_t addr)
{
	// set the axis timer state to be current cycle count
	// plus the cycle count when the timer should time out.
	// The internal paddle read routine reads every 11ms
	for (auto i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
		int16_t value = SDL_GameControllerGetAxis(Controllers[0].m_gc, Controllers[0].m_axis[i]);
		uint8_t val_uint8 =  (uint8_t)(floor((float)(value + 32768) * (float)(255.0f / 65536.0f)));
		Controllers[0].m_axis_timer_state[i] = Total_cycles + uint32_t(val_uint8 * Joystick_cycles_scale);
	}
	return 0;
}

void joystick_reset_write_handler(uint16_t addr, uint8_t val)
{
	joystick_reset_read_handler(addr);
}

void joystick_init()
{
	// load game controller mappings
	int num_mappings = SDL_GameControllerAddMappingsFromFile(Game_controller_mapping_file);
	if (num_mappings == -1) {
		printf("Unable to load mappings file %s for SDL game controllers.\n", Game_controller_mapping_file);
		return;
	}

	Num_controllers = SDL_NumJoysticks();
	for (auto i = 0; i < Num_controllers; i++) {
		if (SDL_IsGameController(i)) {
			Controllers[i].m_gc = SDL_GameControllerOpen(i);
			if (Controllers[i].m_gc != nullptr) {
				Controllers[i].m_initialized = true;
				Controllers[i].m_name = SDL_GameControllerNameForIndex(i);

				for (auto j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++) {
					Controllers[i].m_button_state[j] = 0;
					Controllers[i].m_buttons[j] = static_cast<SDL_GameControllerButton>(static_cast<int>(SDL_CONTROLLER_BUTTON_A) + j);
				}
				for (auto j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++) {
					Controllers[i].m_axis_state[j] = 0;
					Controllers[i].m_axis_timer_state[j] = 0;
					Controllers[i].m_axis[j] = static_cast<SDL_GameControllerAxis>(static_cast<int>(SDL_CONTROLLER_AXIS_LEFTX) + j);
				}
			}
		}
	}

	// register the read/write handlers 
	for (auto i = 0x61; i < 0x67; i++) {
		memory_register_c000_handler(i, joystick_read_handler, joystick_write_handler);
	}
	memory_register_c000_handler(0x70, joystick_reset_read_handler, joystick_reset_write_handler);
}

void joystick_shutdown()
{
	for (auto i = 0; i < Num_controllers; i++) {
		if (Controllers[i].m_initialized == true) {
			SDL_GameControllerClose(Controllers[i].m_gc);
		}
	}
}
