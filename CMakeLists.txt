#
#  CMake file for Apple2emu
#
cmake_minimum_required(VERSION 3.17.1)
project(apple2emu CXX C)

# Directory for output files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
 CACHE PATH "Output directory for executables and DLL's.")

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
   string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
   set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}" CACHE PATH "" FORCE)
endforeach()

# need to pull in find scripts for SDL and SDL image
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# set the following to avoid warnings about SDL2main
set(FPHSA_NAME_MISMATCHED TRUE)

# here are the packages needed to build apple2emu
find_package(SDL2 REQUIRED)
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

macro(add_clean_target dir)
add_custom_target(clean-${dir} COMMAND ${CMAKE_MAKE_PROGRAM} clean WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${dir})
endmacro(add_clean_target)

add_clean_target (imgui)
add_clean_target (nativefiledialog)
add_clean_target (libclipboard)

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

target_include_directories(apple2emu PUBLIC imgui)
target_include_directories(apple2emu PUBLIC nativefiledialog/src/include)
target_include_directories(apple2emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libclipboard/include)
target_include_directories(apple2emu PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libclipboard/include)

set_target_properties(apple2emu PROPERTIES DEBUG_POSTFIX "_debug")

target_link_libraries(apple2emu
   ${CURSES_LIBRARIES}
   imgui
   nfd
   clipboard
   SDL2::Core
   SDL2::Image
   OpenGL::GL
   GLEW::GLEW)

get_filename_component(SDL2_DLL_PATH ${SDL2_LIBRARY} DIRECTORY)
get_filename_component(SDL2_IMAGE_DLL_PATH ${SDL2_IMAGE_LIBRARY} DIRECTORY)

# add post build step to copy extra files into
# the build folder
set (DIRS_TO_CREATE "interface" "roms" "fonts" "symtables")
set (SOURCE_DIRS ${DIRS_TO_CREATE})
list(TRANSFORM DIRS_TO_CREATE PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
list(TRANSFORM SOURCE_DIRS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
foreach(DIR SRC IN ZIP_LISTS DIRS_TO_CREATE SOURCE_DIRS)
   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${DIR})
   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${SRC} ${DIR})
endforeach()

add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt" "${CMAKE_CURRENT_BINARY_DIR}")

if (WIN32)
	add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_DLL_PATH}/SDL2.dll" "${CMAKE_CURRENT_BINARY_DIR}")
	if (SDL2_IMAGE_DLL_PATH)
      set (FILES_TO_COPY "SDL2_image.dll" "libjpeg-9.dll" "libpng16-16.dll" "zlib1.dll")
      list(TRANSFORM FILES_TO_COPY PREPEND "${SDL2_IMAGE_DLL_PATH}/")
      foreach(FILE ${FILES_TO_COPY})
		   add_custom_command(TARGET apple2emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILE} ${CMAKE_CURRENT_BINARY_DIR})
      endforeach()
	endif()
endif()
