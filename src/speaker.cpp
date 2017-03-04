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
#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "speaker.h"
#include "memory.h"

static SDL_AudioDeviceID Device_id = 0;
static SDL_AudioSpec Audio_spec;

const static int Sound_freq = 11025;
const static int Sound_num_channels = 1;
const static int Sound_buffer_size = (Sound_freq * Sound_num_channels) / 60;
const static int Speaker_sample_cycles = CYCLES_PER_FRAME / Sound_buffer_size;

static bool Speaker_on = false;
static int Speaker_cycles = 0;

static int8_t Sound_buffer[Sound_buffer_size];
static int Sound_buffer_index = 0;

uint8_t speaker_soft_switch_handler(uint16_t addr, uint8_t val, bool write)
{
	Speaker_on = !Speaker_on;
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
	SDL_AudioSpec want;

	want.channels = Sound_num_channels;
	want.format = AUDIO_S8;
	want.freq = Sound_freq;
	want.samples = Sound_buffer_size;
	want.callback = nullptr;
	want.userdata = nullptr;

	Device_id = SDL_OpenAudioDevice(nullptr, 0, &want, &Audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
	SDL_PauseAudioDevice(Device_id, 1);

	// set up sound buffer
	Speaker_cycles = 0;
	Sound_buffer_index = 0;
	for (auto i = 0; i < Sound_buffer_size; i++) {
		Sound_buffer[i] = 0;
	}
	Speaker_on = false;
}

void speaker_shutdown()
{
	if (Device_id != 0) {
		SDL_CloseAudioDevice(Device_id);
	}
}

// update internal speaker code, which will first entail
// writing out information on current speaker status to
// the buffer, and then, if full, copying the buffer to
// the SDL audio queue
void speaker_update(int cycles)
{
	static bool last_value = false;
	UNREFERENCED(cycles);

	Speaker_cycles += cycles;
	if (Speaker_cycles < Speaker_sample_cycles) {
		return;
	}
	Speaker_cycles -= Speaker_sample_cycles;
	last_value = Speaker_on;

	// assume speaker on
	int8_t val = 127;
	if (Speaker_on == false) {
		// ramp down value slightly
		val = last_value ? 63 : 0;
	}
	Sound_buffer[Sound_buffer_index++] = val;
	if (Sound_buffer_index == Sound_buffer_size) {
		SDL_QueueAudio(Device_id, Sound_buffer, Sound_buffer_size);
		Sound_buffer_index = 0;
	}
}

void speaker_queue_audio()
{
}

void speaker_pause()
{
	SDL_PauseAudioDevice(Device_id, 1);
}

void speaker_unpause()
{
	SDL_PauseAudioDevice(Device_id, 0);
}
