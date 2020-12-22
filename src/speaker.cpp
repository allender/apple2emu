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

#include <limits.h>

#include "SDL.h"
#include "apple2emu.h"
#include "apple2emu_defs.h"
#include "speaker.h"
#include "memory.h"

static SDL_AudioDeviceID Device_id = 0;
static SDL_AudioSpec Audio_spec;

const static int Sound_samples = 22050;
const static int Sound_num_channels = 1;

// buffer size set to 512, which, while hard coded
// is chosen because SDL requires a power ot 2 for
// the sound buffer and we can make sure that
// we still this buffer at the correcct rate
const static int Sound_buffer_size = 512;

// Speaker_sample_cycle_count tells us how often we need to fill
// buffer (in ms).  Cycles_per_frame is number of cycles per
// millisecond (roughly).  Sound_samples / 60 is how often we need to fill
// the buffer (since we assume the emulator runs at 60fps). 
// cycles_per_frame / (Sound_samples / 60) will tell us how often
// we must fill the sound buffer in apple cycle counts
const int Speaker_sample_cycle_count = Cycles_per_frame / (Sound_samples / Frames_per_second);

static int8_t Sound_silence;
static float Sound_volume = 0.5;

// ring bugger
const static int Sound_ring_buffer_size = Sound_buffer_size * 3;
static int8_t Sound_ring_buffer[Sound_ring_buffer_size];
uint32_t Tail_index, Head_index;

static bool Speaker_on = false;
static int Speaker_cycles = 0;

static void speaker_callback(void *userdata, uint8_t *stream, int len)
{
	int index = 0;
	while (Head_index < Tail_index) {
		stream[index] = uint8_t(Sound_ring_buffer[(Head_index++) % Sound_ring_buffer_size] * Sound_volume);
		index++;
		if (index == len) {
			break;
		}
	}
	uint8_t val = stream[index-1];
	while (index < len) {
		// stream[index++] = Sound_silence;
		stream[index++] = val;
	}
}

uint8_t speaker_soft_switch_handler(uint16_t addr, uint8_t val, bool write)
{
	Speaker_on = !Speaker_on;
	UNREFERENCED(addr);
	UNREFERENCED(val);
	UNREFERENCED(write);

	return memory_read_floating_bus();
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
	want.freq = Sound_samples;
	want.samples = Sound_buffer_size;
	want.callback = speaker_callback;
	// want.callback = nullptr;

	Device_id = SDL_OpenAudioDevice(nullptr, 0, &want, &Audio_spec, 0);
	if (Device_id == 0) {
		printf("Unable to get valid SDL Audio device: %s\n", SDL_GetError());
		return;
	}

	SDL_PauseAudioDevice(Device_id, 1);

	// set up sound buffer
	Speaker_cycles = 0;

	Tail_index = Head_index = 0;
	for (auto i = 0; i < Sound_ring_buffer_size; i++) {
		Sound_ring_buffer[i] = 0;
	}
	Speaker_on = false;

	Sound_silence = SCHAR_MIN;
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
	Speaker_cycles += cycles;
	if (Speaker_cycles < Speaker_sample_cycle_count) {
		return;
	}
	Speaker_cycles -= Speaker_sample_cycle_count;

	// assume speaker on
	int8_t val = Sound_silence;
	if (Speaker_on) {
		val = ~val;
	}

	// ring buffer
	SDL_LockAudioDevice(Device_id);
	Sound_ring_buffer[(Tail_index++) % Sound_ring_buffer_size] = val;
	SDL_assert(Tail_index - Head_index < Sound_ring_buffer_size);
	SDL_UnlockAudioDevice(Device_id);

	// fill the buffer if we have reached that point
	// if (Tail_index - Head_index >= Sound_buffer_size) {
	// 	uint8_t buffer[Sound_buffer_size];

	// 	auto index = 0;
	// 	while (Head_index < Tail_index && index < Sound_buffer_size) {
	// 		SDL_assert(index< Sound_buffer_size);
	// 		buffer[index++] = uint8_t(Sound_ring_buffer[(Head_index++) % Sound_ring_buffer_size] * Sound_volume);
	// 	}
	// 	SDL_QueueAudio(Device_id, buffer, Sound_buffer_size);
	// }
}

void speaker_pause()
{
	SDL_PauseAudioDevice(Device_id, 1);
}

void speaker_unpause()
{
	SDL_PauseAudioDevice(Device_id, 0);
}

void speaker_set_volume(int volume)
{
	SDL_assert(volume >= 0 && volume <= 100);

	// to percentage, then 1/4th of that to make
	// even more quiet (lower max volume)
	Sound_volume = volume / 100.0f / 4.0f;
}
