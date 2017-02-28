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
#include "speaker.h"
#include "memory.h"

SDL_AudioDeviceID Device_id = 0;

uint8_t speaker_soft_switch_handler(uint16_t addr, uint8_t val, bool write)
{
	UNREFERENCED(addr);
	UNREFERENCED(val);
	UNREFERENCED(write);
	return 0xff;
}

// initialize the speaker system.  For now, this is just setting up a handler
// for the memory location that will do nothing
void speaker_init()
{
	// allow for re-entrancy
	if (Device_id != 0) {
		SDL_CloseAudioDevice(Device_id);
	}

	// open up sdl audio device to write wave data
	SDL_AudioSpec want, have;

	want.channels = 1;
	want.format = AUDIO_S8;
	want.freq = 11025;
	want.samples = 4096;
	want.callback = nullptr;

	Device_id = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
	SDL_PauseAudioDevice(Device_id, 0);
}

void speaker_shutdown()
{
	if (Device_id != 0) {
		SDL_CloseAudioDevice(Device_id);
	}
}
