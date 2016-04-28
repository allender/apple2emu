#include <stdint.h>
#include "6502/cpu.h"
#include "6502/memory.h"

// includes for easylogging.  Defines need to be set up
// before header is included
#define ELPP_THREAD_SAFE
#define ELPP_DEFAULT_LOG_FILE "logs\\apple2emu.log"
//#define ELPP_STACKTRACE_ON_CRASH
#include "utils/easylogging++.h"

#define USE_SDL

#define LITTLE_ENDIAN

// turn off deprecated warnings
#pragma warning(disable:4996)

#define _CRT_SECURE_NO_WARNINGS

// timings - these need to get moved elsewhere eventually.

// master crystal is 14.31818 Mhz.  CPU clock is 
const float Clock_master = 14.31818f;
const float CLock_6502 = ((Clock_master * 65.0f) / 912.0f);

