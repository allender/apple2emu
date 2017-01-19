#
# Cmake for building nativefiledialog
#
if (WIN32)
   set(PLATFORM_CPP nfd_win.cpp)
elseif (CMAKE_HOST_APPLE)
   set(PLATFORM_CPP nfd_cocoa.m)
else()
   # for unix systems, make sure that we have
   # the gtk include directories available
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
   include_directories(${GTK3_INCLUDE_DIRS})
   set(PLATFORM_CPP nfd_gtk.cpp)
endif()

include_directories(include)
add_library(nfd nfd_common.cpp ${PLATFORM_CPP})
target_link_libraries(nfd ${GTK3_LIBRARIES})

