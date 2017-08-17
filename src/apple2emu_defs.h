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

#include <stdint.h>

// pull this assert definition in everywhere
#include <SDL_assert.h>

#pragma once

// get past stricmp problems
#if !defined(_WIN32) && !defined(_WIN64)
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#if defined(_WIN32) || defined(_WIN64)

// turn off deprecated warnings
#pragma warning(disable:4996)

// turn off warnings for unused functions
#pragma warning(disable:4505)


#endif

// define for helping to clear out unreferenced
// parameter warmings
#define UNREFERENCED(X) (void)X

// timings - these need to get moved elsewhere eventually.

// master crystal is 14.31818 Mhz.  CPU clock is
const float Clock_master = 14.31818f;
const float Clock_6502 = ((Clock_master * 65.0f) / 912.0f);
const float Clock_z80 = (Clock_6502 * 2);
const float Z80_clock_multiplier = Clock_z80 / Clock_6502;
