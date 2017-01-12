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
#include "6502/cpu.h"
#include "6502/memory.h"

#define USE_SDL

#if defined(LITTLE_ENDIAN)
#undef LITTLE_ENDIAN
#endif

#define LITTLE_ENDIAN

// define for helping to clear out unreferenced
// parameter warmings
#define UNREFERENCED(X) X

// get past stricmp problems
#if !defined(_WIN32) && !defined(_WIN64)
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

// turn off deprecated warnings
#if defined(_WIN32) || defined(_WIN64)
#pragma warning(disable:4996)
#endif

#define _CRT_SECURE_NO_WARNINGS

// timings - these need to get moved elsewhere eventually.

// master crystal is 14.31818 Mhz.  CPU clock is 
const float Clock_master = 14.31818f;
const float CLock_6502 = ((Clock_master * 65.0f) / 912.0f);

