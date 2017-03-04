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

static SDL_AudioDeviceID Device_id = 0;
static SDL_AudioSpec Audio_spec;

const static int Sound_freq = 11025;
const static int Sound_num_channels = 1;
const static int Sound_buffer_size = (Sound_freq * Sound_num_channels) / 30;
const static int Speaker_sample_cycles = 183;

static bool Speaker_on = false;
static int Speaker_cycles = 0;


// use a ring buffer to store the data from the speaker.
struct ring_buffer {
	int     head;     // head of the ring buffer. start the current set of data
	int     tail;     // end of the ring buffer.  Writes go here
	int     size;     // size of the finbuffer
	int8_t *data;     // data for the bufffer;
};

ring_buffer Audio_ring_buffer;

/*
// callback from SDL for audio
static void audio_callback(void *userdata, uint8_t *stream, int len)
{
	ring_buffer *rb = (ring_buffer *)userdata;

	memset(stream, 0, len);
	for (auto i = 0; i < len && rb->tail != rb->head; i++) {
		stream[i] = rb->data[(rb->head++) % rb->size];
	}
}
*/

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
	//want.callback = audio_callback;
	want.userdata = (void *)&Audio_ring_buffer;

	Device_id = SDL_OpenAudioDevice(nullptr, 0, &want, &Audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
	SDL_PauseAudioDevice(Device_id, 1);

	// set up sound buffer
	Speaker_cycles = 0;
	Audio_ring_buffer.size = Sound_buffer_size * 2;
	Audio_ring_buffer.data = new int8_t[Audio_ring_buffer.size];
	Audio_ring_buffer.head = 0;
	Audio_ring_buffer.tail = 0;
	Speaker_on = false;
}

void speaker_shutdown()
{
	if (Device_id != 0) {
		SDL_CloseAudioDevice(Device_id);
	}

	// delete the ring buffer;
	if (Audio_ring_buffer.data != nullptr) {
		delete[] Audio_ring_buffer.data;
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

	//SDL_LockAudioDevice(Device_id);

	// sanity check for the ring buffer
	ASSERT(Audio_ring_buffer.tail - Audio_ring_buffer.head < Audio_ring_buffer.size);

	// assume speaker on
	int8_t val = 127;
	if (Speaker_on == false) {
		// ramp down value slightly
		val = last_value ? 63 : 0;
	}
	//Audio_ring_buffer.data[(Audio_ring_buffer.tail++) % Audio_ring_buffer.size] = val;
	if (Audio_ring_buffer.tail - Audio_ring_buffer.head == Audio_ring_buffer.size) {
		//char int8_t buffer[Audio_ring_buffer.size];

		//for (i = 0; i < Audio_ring_buffer.size; i++) {
		//	buffer[
	}

	//SDL_UnlockAudioDevice(Device_id);
}

void speaker_pause()
{
	SDL_PauseAudioDevice(Device_id, 1);
}

void speaker_unpause()
{
	SDL_PauseAudioDevice(Device_id, 0);
}
