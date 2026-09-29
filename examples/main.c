// Minimal TCMIPS example: compile this C file to a .tcm image and run it on the
// TCMIPS emulator (or load it in the Turing Complete File Loader). The program's
// printf output is forwarded to the host via semihost stdio.
//
// Build (the CMakeLists.txt locates the bundled toolchain automatically; see
// SDK README for details):
//   cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
//   cmake --build build
//
// Run (the window stays open after the guest exits by default):
//   ../emulator/TCMIPSEmulator build/hello.tcm
// Add -hold=0 to auto-close the window when the guest exits (scripts / CI):
//   ../emulator/TCMIPSEmulator build/hello.tcm -hold=0

#include <stdio.h>

int main(void) {
  printf("Hello, world!\n");
  return 0;
}