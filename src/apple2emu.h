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

enum class emulator_state : uint8_t {
	SPLASH_SCREEN,
	EMULATOR_STARTED,
};

enum class emulator_type: uint8_t {
	APPLE2 = 0,
	APPLE2_PLUS,
	APPLE2E,
	APPLE2E_ENHANCED,

	NUM_EMULATOR_TYPES,
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
const uint32_t HORZ_STATE_COUNTER = 65;
const uint32_t VERT_STATE_COUNTER = 262;
const uint32_t CYCLES_PER_FRAME = VERT_STATE_COUNTER * HORZ_STATE_COUNTER;

// several externs for cycle counting
extern uint32_t Total_cycles;
extern uint32_t Total_cycles_this_frame;

// globals for controlling the emulator.  Tied into interface
extern uint32_t Speed_multiplier;
extern bool Auto_start;
extern emulator_state Emulator_state;
extern emulator_type Emulator_type;

void reset_machine();
