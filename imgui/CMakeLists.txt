#
# Cmake for building imgui
#

# subdirectories to build
include_directories(${SDL2_INCLUDE_DIR})

add_library(imgui imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_impl_sdl.cpp)

