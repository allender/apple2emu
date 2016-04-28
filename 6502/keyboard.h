

// keyboard handling (through SDL)

#pragma once

void keyboard_init();
void keyboard_shutdown();
void keyboard_handle_event(SDL_Event &evt);
uint32_t keyboard_get_key();
