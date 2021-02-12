#
#  CMake file for Apple2emu
#
cmake_minimum_required(VERSION 3.5.1)
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

set (CMAKE_CXX_STANDARD 17)

# deal with compiler flags
if (MSVC)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
   set (CMAKE_CXX_FLAGS "-Wall -Werror -Wno-multichar ${CMAKE_CXX_FLAGS}")
endif(MSVC)

# with SDL, we are supplying our own main
add_definitions(-DSDL_MAIN_HANDLED)

# subdirectories to build
add_subdirectory(imgui)
add_subdirectory(nativefiledialog)
add_subdirectory(libclipboard)

# add in other folders that are needed for ncludes
include_directories(
   ${SDL2_INCLUDE_DIR}
   ${SDL2_IMAGE_INCLUDE_DIR}
   imgui
   nativefiledialog/src/include
   ${CMAKE_CURRENT_SOURCE_DIR}/libclipboard/include
   ${CMAKE_CURRENT_BINARY_DIR}/libclipboard/include
   z80emu)

include_directories(SYSTEM
   ${GLEW_INCLUDE_DIR}
   ${OPENGL_INCLUDE_DIR})

set (EMU_SOURCES
   src/apple2emu.cpp
   src/6502.cpp
   src/debugger.cpp
   src/debugger_console.cpp
   src/debugger_disasm.cpp
   src/debugger_memory.cpp
   src/disk.cpp
   src/disk_image.cpp
   src/font.cpp
   src/interface.cpp
   src/joystick.cpp
   src/keyboard.cpp
   src/memory.cpp
   src/path_utils.cpp
   src/speaker.cpp
   src/video.cpp
   src/z80softcard.cpp
   z80emu/z80emu.cpp)

add_executable(apple2emu ${EMU_SOURCES})
set_target_properties(apple2emu PROPERTIES DEBUG_POSTFIX "_debug")
target_link_libraries(apple2emu
   ${CURSES_LIBRARIES}
   imgui
   nfd
   clipboard
   ${SDL2_IMAGE_LIBRARY}
   ${SDL2_LIBRARY}
   ${GLEW_LIBRARIES}
   ${OPENGL_LIBRARIES})

get_filename_component(SDL2_DLL_PATH ${SDL2_LIBRARY} DIRECTORY)
get_filename_component(SDL2_IMAGE_DLL_PATH ${SDL2_IMAGE_LIBRARY} DIRECTORY)

# add post build step to copy extra files into
# the build folder
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/interface")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/disks")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/roms")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/fonts")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/symtables")
if (WIN32)
	add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_DLL_PATH}/SDL2.dll" "${CMAKE_CURRENT_BINARY_DIR}")
	if (SDL2_IMAGE_DLL_PATH)
		add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_IMAGE_DLL_PATH}/SDL2_image.dll" "${CMAKE_CURRENT_BINARY_DIR}")
		add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_IMAGE_DLL_PATH}/libjpeg-9.dll" "${CMAKE_CURRENT_BINARY_DIR}")
		add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_IMAGE_DLL_PATH}/libpng16-16.dll" "${CMAKE_CURRENT_BINARY_DIR}")
		add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_IMAGE_DLL_PATH}/zlib1.dll" "${CMAKE_CURRENT_BINARY_DIR}")
	endif()
endif()
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt" "${CMAKE_CURRENT_BINARY_DIR}")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/interface" "${CMAKE_CURRENT_BINARY_DIR}/interface")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/disks" "${CMAKE_CURRENT_BINARY_DIR}/disks")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/roms" "${CMAKE_CURRENT_BINARY_DIR}/roms")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/fonts" "${CMAKE_CURRENT_BINARY_DIR}/fonts")
add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/symtables" "${CMAKE_CURRENT_BINARY_DIR}/symtables")

