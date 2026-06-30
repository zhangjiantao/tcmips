cmake_minimum_required(VERSION 3.31)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake"
    CACHE FILEPATH "Path to toolchain file" FORCE)

project(tcmips_devkit)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (NOT DEFINED TCM_CONSOLE_CUSTOM_FONT_8X16)
  set(TCM_CONSOLE_CUSTOM_FONT_8X16 ON)
endif ()

if (NOT DEFINED TCM_CONSOLE_CUSTOM_FONT_16X32)
  set(TCM_CONSOLE_CUSTOM_FONT_16X32 OFF)
endif ()

if (NOT DEFINED TCM_BUILD_TEST)
  set(TCM_BUILD_TEST OFF)
endif ()

set(TCM_MEMORY_SIZE 0x04000000 CACHE STRING "Total size of TCM main memory" FORCE)
set(TCM_VRAM_ASCII_SIZE 0x100000 CACHE STRING "VRAM size for ASCII character display mode" FORCE)
set(TCM_VRAM_PIXEL_SIZE 0x300000 CACHE STRING "VRAM size for raw pixel framebuffer mode" FORCE)
set(TCM_STACK_SIZE 0x40000 CACHE STRING "Stack size for TCM" FORCE)
set(TCM_GUARD_PAGE_SIZE 0x1000 CACHE STRING "Guard page size for TCM" FORCE)

set(TCM_VRAM_PIXEL_ADDR 0x03D00000 CACHE STRING "Pixel VRAM start address" FORCE)
set(TCM_VRAM_ASCII_ADDR 0x03C00000 CACHE STRING "ASCII VRAM start address" FORCE)
set(TCM_STACK_TOP_ADDR 0x03C00000 CACHE STRING "Stack top pointer (Initial SP)" FORCE)
set(TCM_STACK_LIMIT_ADDR 0x03BC0000 CACHE STRING "Stack bottom address" FORCE)
set(TCM_GUARD_PAGE_ADDR 0x03BBF000 CACHE STRING "Guard page start address" FORCE)
set(TCM_HEAP_LIMIT_ADDR 0x03BBF000 CACHE STRING "Maximum limit address for Heap" FORCE)

add_subdirectory(tcmips)

add_subdirectory(demo)

if (TCM_BUILD_TEST)
  add_subdirectory(test)
endif ()
