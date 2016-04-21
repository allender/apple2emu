#pragma once

#include "6502/cpu.h"
#include "6502/memory.h"

void debugger_enter();
void debugger_init();
void debugger_process(cpu_6502& cpu, memory& mem);