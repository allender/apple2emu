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
#include "joystick.h"

static int Num_controllers;

// information for joysticks
class controller {
public:
	bool                     m_initialized;   // is this joystick initialized
	SDL_GameController      *m_gc;            // pointer to game controller structure
	int8_t                   m_button_state[SDL_CONTROLLER_BUTTON_MAX];
	int16_t                  m_axis_state[SDL_CONTROLLER_AXIS_MAX];
	SDL_GameControllerButton m_buttons[SDL_CONTROLLER_BUTTON_MAX];  // mapping from button number to button enum
	SDL_GameControllerAxis   m_axis[SDL_CONTROLLER_AXIS_MAX];       // mapping from axis number to axis enum

	controller() :m_initialized(false) {};

};

const int Max_controllers = 16;
controller Controllers[Max_controllers];

// reads the status of button num from the joystick structures
static uint8_t joystick_read_button(uint8_t button_num)
{
	uint8_t val = SDL_GameControllerGetButton(Controllers[0].m_gc, Controllers[0].m_buttons[button_num]);
	return val;
}

static uint8_t joystick_read_axis(uint8_t axis_num)
{
	int16_t value = SDL_GameControllerGetAxis(Controllers[0].m_gc, Controllers[0].m_axis[axis_num]);
	uint8_t ret_value =  (uint8_t)(floor((float)(value + 32768) * (float)(255.0f / 65536.0f)));
	return ret_value;
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

}

void joystick_init()
{
	Num_controllers = SDL_NumJoysticks();
	for (auto i = 0; i < Num_controllers; i++) {
		if (SDL_IsGameController(i)) {
			Controllers[i].m_gc = SDL_GameControllerOpen(i);
			if (Controllers[i].m_gc != nullptr) {
				Controllers[i].m_initialized = true;
				for (auto j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++) {
					Controllers[i].m_button_state[j] = 0;
					Controllers[i].m_buttons[j] = static_cast<SDL_GameControllerButton>(static_cast<int>(SDL_CONTROLLER_BUTTON_A) + j);
				}
				for (auto j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++) {
					Controllers[i].m_axis_state[j] = 0;
					Controllers[i].m_axis[j] = static_cast<SDL_GameControllerAxis>(static_cast<int>(SDL_CONTROLLER_AXIS_LEFTX) + j);
				}
			}
		}
	}

	// register the read/write handlers 
	for (auto i = 0x61; i < 0x67; i++) {
		memory_register_c000_handler(i, joystick_read_handler, joystick_write_handler);
	}
}

void joystick_shutdown()
{
	for (auto i = 0; i < Num_controllers; i++) {
		if (Controllers[i].m_initialized == true) {
			SDL_GameControllerClose(Controllers[i].m_gc);
		}
	}
}

void joystick_handle_axis(SDL_Event &evt)
{
	uint32_t joy_num = evt.caxis.which  - 1;
	int32_t value = evt.caxis.value;
	value = (int)((float)(value + 32768) * (float)(255.0f / 65536.0f));
	Controllers[joy_num].m_axis_state[evt.caxis.axis] = value;
}

void joystick_handle_button(SDL_Event &evt)
{
	uint32_t joy_num = evt.cbutton.which;
	Controllers[joy_num].m_button_state[evt.cbutton.button] = evt.cbutton.state;
}
