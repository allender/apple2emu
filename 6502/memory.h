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

#if !defined(MEMORY_H)
#define MEMORY_H

#include <stdint.h>

typedef std::function<uint8_t(uint16_t, uint8_t, bool)> soft_switch_function;

void memory_init();
void memory_shutdown();
bool memory_load_buffer(uint8_t *buffer, uint16_t size, uint16_t location);
uint8_t memory_read_aux(uint16_t addr);
uint8_t memory_read(uint16_t addr);
void memory_write(uint16_t addr, uint8_t val);
void memory_set_paging_tables();
void memory_register_slot_handler(const uint8_t slot, soft_switch_function func, uint8_t *expansion_rom = nullptr);

#endif  // MEMORY_H
