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

#pragma once
#include <SDL_log.h>
#include "6502.h"

enum class emulator_state : uint8_t {
	SPLASH_SCREEN,
	EMULATOR_STARTED,
	EMULATOR_PAUSED,
	EMULATOR_TEST,
};

enum class emulator_type: uint8_t {
	APPLE2 = 0,
	APPLE2_PLUS,
	APPLE2E,
	APPLE2E_ENHANCED,

	NUM_EMULATOR_TYPES,
};

// enums for logginf
enum {
	LOG_CATEGORY_DISK = SDL_LOG_CATEGORY_CUSTOM
};

extern const char *Emulator_names[static_cast<uint8_t>(emulator_type::NUM_EMULATOR_TYPES)];

// constants for machine speeds
const double CLOCK_14M = 14318181.8181;

// number of clock cycles per video refresh time.  PIease please
// read Understanding the Apple ][  Page 3-18 (in the whole
// chapter in fact) for information on this and other timing
// related items.
const double FREQ_6502 = (CLOCK_14M * 65.0) / ((65.0 * 14) + 2);
const uint32_t FREQ_6502_MS = (uint32_t)((CLOCK_14M * 65.0) / ((65.0 * 14) + 2) / 1000.0f);

// timing related to video refresh.  See Understanding the
// Apple ][ page 3-11.
const uint32_t Horz_state_counter = 65;
const uint32_t Horz_clock_start = 24;        // clock where HBL starts
const uint32_t Horz_clock_preset = 41;       // clock when reset happens
const uint32_t Horz_segment_count = 40;
const uint32_t Horz_scan_preset = 64;
const uint32_t Vert_state_counter = 262;
const uint32_t Vert_line_start = 256;        // vert line after preset
const int32_t Vert_line_preset = 256;       // vert line when preset happens
const uint32_t Vert_line_preset_value = 511; // value of vert state on preset (b111111111)
const uint32_t Vert_preset_value = 250;      // preset value (b11111010)

const uint32_t Cycles_per_frame = Vert_state_counter * Horz_state_counter;

// several externs for cycle counting
extern uint32_t Total_cycles;
extern uint32_t Total_cycles_this_frame;

// globals for controlling the emulator.  Tied into interface
extern uint32_t Speed_multiplier;
extern bool Auto_start;
extern emulator_state Emulator_state;
extern emulator_type Emulator_type;

extern cpu_6502 cpu;

void reset_machine();
