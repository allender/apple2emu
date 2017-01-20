#
#  CMake file for Apple2emu
#
cmake_minimum_required(VERSION 3.6.0)
project(apple2emu CXX)

# Directory for output files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
 CACHE PATH "Output directory for executables and DLL's.")

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
   string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
   set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}" CACHE PATH "" FORCE)
endforeach()

# need to pull in find scripts for SDL and SDL image
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# here are the packages needed to build apple2emu
find_package(SDL2 REQUIRED sdl2)
find_package(SDL2_image REQUIRED SDL2_image>=2.0.0)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

# win32 builds will use pdcurses locally while
# other builds will require ncurses
if (WIN32)
   set (CURSES_LIBRARIES ${PROJECT_SOURCE_DIR}/pdcurses/pdcurses.lib ${PROJECT_SOURCE_DIR}/pdcurses/panel.lib)
else()
   set(CURSES_NEED_NCURSES TRUE)
   find_package(curses REQUIRED)
   set (CURSES_LIBS ncurses)
endif()

set (CMAKE_CXX_STANDARD 11)

# deal with compiler flags
if (MSVC)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
   set (CMAKE_CXX_FLAGS "-Wall -Werror -Wno-multichar ${CMAKE_CXX_FLAGS}")
endif(MSVC)

# subdirectories to build
subdirs(imgui nativefiledialog)

#this is needed for imgui and the emulator
include_directories(
   ${SDL2_INCLUDE_DIR}
   ${SDL2_IMAGE_INCLUDE_DIR}
   imgui
   nativefiledialog/src/include
   pdcurses)

include_directories(SYSTEM
   ${GLEW_INCLUDE_DIR}
   ${OPENGL_INCLUDE_DIR})

set (EMU_SOURCES
   src/apple2emu.cpp
   src/cpu.cpp
   src/debugger.cpp
   src/disk.cpp
   src/disk_image.cpp
   src/font.cpp
   src/interface.cpp
   src/joystick.cpp
   src/keyboard.cpp
   src/memory.cpp
   src/path_utils.cpp
   src/speaker.cpp
   src/video.cpp)

add_executable(apple2emu ${EMU_SOURCES})
set_target_properties(apple2emu PROPERTIES DEBUG_POSTFIX "_debug")
target_link_libraries(apple2emu
   ${CURSES_LIBRARIES}
   imgui
   nfd
   ${SDL2_IMAGE_LIBRARY}
   ${SDL2_LIBRARY}
   ${GLEW_LIBRARIES}
   ${OPENGL_LIBRARIES})

# add post build step to copy extra files into
# the build folder
if (WIN32)
   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/SDL2.dll" "${CMAKE_CURRENT_BINARY_DIR}")
   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/SDL2_image.dll" "${CMAKE_CURRENT_BINARY_DIR}")
   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/libjpeg-9.dll" "${CMAKE_CURRENT_BINARY_DIR}")
endif()
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt" "${CMAKE_CURRENT_BINARY_DIR}")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/splash.jpg" "${CMAKE_CURRENT_BINARY_DIR}")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/disks" "${CMAKE_CURRENT_BINARY_DIR}/disks")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/roms" "${CMAKE_CURRENT_BINARY_DIR}/roms")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/fonts" "${CMAKE_CURRENT_BINARY_DIR}/fonts")

