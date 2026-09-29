# Minimal TCMIPS example project.
#
# This is an independent "user project": it is NOT add_subdirectory'd by the
# tcmips-devkit tree. After unpacking the SDK, this example sits next to the
# bundled toolchain/ directory, so the cross toolchain is located relative to
# this file below, and the project builds out of the box with a plain
#   cmake -G Ninja -S . -B build
# (no need to pass -DCMAKE_TOOLCHAIN_FILE=... on the command line).
#
# The modified toolchain binds the sysroot runtime/standard libraries
# automatically, so no explicit target_link_libraries(tcmips_core ...) is
# required here.

cmake_minimum_required(VERSION 3.31)

# Locate the bundled cross toolchain relative to this CMakeLists.txt
# (examples/ -> up one level to the SDK root -> toolchains/). Set it as a cache
# variable before project() so CMake loads it as the toolchain file.
if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      "${CMAKE_CURRENT_LIST_DIR}/../toolchains/cmake/tcmips.toolchain.cmake"
      CACHE FILEPATH "TCMIPS cross toolchain file" FORCE)
endif()

project(hello C)

add_executable(hello main.c)