/*
LiteNES originates from Stanislav Yaglo's mynes project:
  https://github.com/yaglo/mynes

LiteNES is a "more portable" version of mynes.
  all system(library)-dependent code resides in "hal.c" and "main.c"
  only depends on libc's memory moving utilities.

How does the emulator work?
  1) read file name at argv[1]
  2) load the rom file into array rom
  3) call fce_load_rom(rom) for parsing
  4) call fce_init for emulator initialization
  5) call fce_run(), which is a non-exiting loop simulating the NES system
  6) when SIGINT signal is received, it kills itself
*/

#include "fce.h"
#include "string.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// static char rom[1048576];


#ifdef ROM_ADVISLAND
#include "rom_maoxiandao.h"
#endif

#ifdef ROM_SUPERMARIO
#include "rom_supermariobros.h"
#endif

#ifdef ROM_CIRCUS
#include "rom_maxituan.h"
#endif

int main(int argc, char *argv[]) {
  if (fce_load_rom((char *)data) != 0) {
    fprintf(stderr, "Invalid or unsupported rom.\n");
    exit(1);
  }
  fce_init();
  fce_run();
  return 0;
}
