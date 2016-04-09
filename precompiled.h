#include "6502/cpu.h"
#include "6502/memory.h"

#include <stdint.h>

#define ELPP_THREAD_SAFE
//#define ELPP_STACKTRACE_ON_CRASH

#include "utils/easylogging++.h"

#define LITTLE_ENDIAN
