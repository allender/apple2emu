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

const static int Sound_samples = 44100;
const static int Sound_num_channels = 1;
const static int Sound_buffer_size = (Sound_samples * Sound_num_channels) / 60;
const static int Speaker_sample_cycles = Cycles_per_frame / Sound_buffer_size;

static int8_t Sound_silence;

// ring bugger
const static int Sound_ring_buffer_size = Sound_buffer_size * 2;
static int8_t Sound_ring_buffer[Sound_ring_buffer_size];
uint32_t Tail_index, Head_index;
SDL_sem *Sound_sem;

static bool Speaker_on = false;
static int Speaker_cycles = 0;

static void speaker_callback(void *userdata, uint8_t *stream, int len)
{
	SDL_semaphore *sem = (SDL_semaphore *)userdata;

	//int total_shorts = len / 2;
	//int tail_index = Tail_index % Sound_ring_buffer_size;
	//int head_index = Head_index % Sound_ring_buffer_size;
	// we have to memcpy since we have shorts, and we must be prepared to
	// wrap the buffer to the beginning
	//if (head_index + total_shorts > Sound_ring_buffer_size) {
	//	int num_samples = Sound_ring_buffer_size - head_index;
	//	memcpy(stream, &Sound_ring_buffer[head_index], num_samples);
	//	memcpy(&stream[num_samples], Sound_ring_buffer, total_shorts - num_samples);
	//} else {
	//	memcpy(stream, &Sound_ring_buffer[head_index], total_shorts);
	//}
	//Head_index += total_shorts;

	SDL_SemWait(sem);

	int index = 0;
	while (Head_index < Tail_index) {
		stream[index] = Sound_ring_buffer[(Head_index++) % Sound_ring_buffer_size];
		index++;
		if (index == len) {
			break;
		}
	}
	SDL_SemPost(sem);
	while (index < len) {
		stream[index++] = Sound_silence;
	}
	SDL_assert(Tail_index >= Head_index);
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

	Sound_sem = SDL_CreateSemaphore(1);
	if (Sound_sem == nullptr) {
		// ummm... have to figure out what to do here
	}

	// open up sdl audio device to write wave data
	SDL_AudioSpec want;

	want.channels = Sound_num_channels;
	want.format = AUDIO_S8;
	want.freq = Sound_samples;
	want.samples = Sound_buffer_size;
	want.callback = speaker_callback;
	want.userdata = Sound_sem;

	Device_id = SDL_OpenAudioDevice(nullptr, 0, &want, &Audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
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
	SDL_DestroySemaphore(Sound_sem);
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

	// assume speaker on
	int8_t val = Sound_silence;
	if (Speaker_on) {
        last_value = last_value;
		val = ~val;
	}

	// ring buffer
	SDL_SemWait(Sound_sem);
	Sound_ring_buffer[(Tail_index++) % Sound_ring_buffer_size] = val;
	SDL_SemPost(Sound_sem);

	SDL_assert(Head_index <= Tail_index);
	last_value = Speaker_on;
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
