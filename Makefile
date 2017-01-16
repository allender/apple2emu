#  Makefile for apple2emi project
#  temporary placeholder under cmake is in place
#
CC = g++
CFLAGS = -g -D_GNU_SOURCE=1 -D_THREAD_SAFE -c -std=c++11 -Wall -Werror -I/Library/Frameworks/SDL2.framework/Headers -I/Library/Frameworks/SDL2_image.framework/Headers -I /usr/local/include -I. -I nativefiledialog/src -I nativefiledialog/src/include -I include -I . -include "precompiled.h" 

MFLAGS = -g -D_GNU_SOURCE=1 -D_THREAD_SAFE -c -Wall -I/usr/local/include/SDL2 -I /usr/local/include -I. -I nativefiledialog/src -I nativefiledialog/src/include -I include -I . 

LDFLAGS = -g -L /usr/local/lib -lncurses -Wl,-framework,Cocoa -framework OpenGL -framework SDL2 -framework SDL2_image -lglew

SOURCES = apple2emu.cpp 6502/joystick.cpp 6502/cpu.cpp 6502/disk.cpp 6502/disk_image.cpp 6502/font.cpp 6502/keyboard.cpp 6502/memory.cpp 6502/opcodes.cpp 6502/speaker.cpp 6502/video.cpp debugger/debugger.cpp utils/path_utils.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_impl_sdl.cpp ui/interface.cpp nativefiledialog/src/nfd_common.cpp 

MSOURCES = nativefiledialog/src/nfd_cocoa.m

OBJECTS = $(SOURCES:.cpp=.o) $(MSOURCES:.m*.o)

EXECUTABLE = apple2emu

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(MOBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) nativefiledialog/src/nfd_cocoa.o  -o $@

.cpp.o:
	$(CC)	$(CFLAGS) $< -o $@

.m.o:
	$(CC)	$(MFLAGS) $< -o $@

clean:
	rm -f apple2emu $(OBJECTS)
