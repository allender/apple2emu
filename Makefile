#  Makefile for apple2emi project
#  temporary placeholder under cmake is in place
#
CC = g++
CFLAGS = -g -D_GNU_SOURCE=1 -D_THREAD_SAFE -c -std=c++11 -Wall -I/usr/local/include/SDL2 -I /usr/local/include -I. -I../easyloggingpp/src -include "precompiled.h"

LDFLAGS = -g -L /usr/local/lib -lSDL2_image -lSDL2 -lncurses -Wl,-framework,Cocoa -framework OpenGL -lglew

SOURCES = apple2emu.cpp 6502/cpu.cpp 6502/disk.cpp 6502/disk_image.cpp 6502/font.cpp 6502/keyboard.cpp 6502/memory.cpp 6502/opcodes.cpp 6502/speaker.cpp 6502/video.cpp debugger/debugger.cpp utils/path_utils.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_impl_sdl.cpp ui/main_menu.cpp

OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = apple2emu

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC)	$(CFLAGS) $< -o $@

clean:
	rm -f apple2emu $(OBJECTS)
