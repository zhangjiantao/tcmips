cmake_minimum_required(VERSION 3.31)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake"
    CACHE FILEPATH "Path to toolchain file" FORCE)

project(tcmips_devkit)

option(TCM_CORE_INLINE "Enable inline optimization for TCM core libs" ON)

option(TCM_BOOTLOADER_SHOW_PROGRESS "Show progress bar during bootloader startup" ON)

set(TCM_MEMORY_SIZE 0x1000000 CACHE STRING "Total size of TCM main memory" FORCE)

set(TCM_STACK_SIZE 0x4000 CACHE STRING "Stack size for TCM" FORCE)

set(TCM_GUARD_PAGE_SIZE 0x1000 CACHE STRING "Guard page size for TCM" FORCE)

add_subdirectory(tcmips)

#if (TCM_BUILD_TOOLCHAIN)
#  add_subdirectory(test)
#endif ()

add_subdirectory(test)
add_subdirectory(demo)
