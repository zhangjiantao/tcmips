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

#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ROM 32
#define ROM_SUFFIX ".nes"

typedef struct {
  char name[256];
  char path[512];
} RomEnt;

static int is_rom(const char *n) {
  size_t l = strlen(n);
  return l > strlen(ROM_SUFFIX) &&
         strcmp(n + l - strlen(ROM_SUFFIX), ROM_SUFFIX) == 0;
}

char *select_rom(size_t *size) {
  RomEnt arr[MAX_ROM];
  int cnt = 0;

  printf("KeyMap:\n");
  printf("JOYCON_A       ---  z\n");
  printf("JOYCON_B       ---  x\n");
  printf("JOYCON_START   ---  Enter\n");
  printf("JOYCON_SELECT  ---  Shift\n\n");

  DIR *d = opendir(".");
  if (!d) {
    perror("opendir");
    return NULL;
  }

  struct dirent *e;
  char cwd[512];
  getcwd(cwd, sizeof cwd);

  printf("ROMs:\n");
  while ((e = readdir(d)) && cnt < MAX_ROM) {
    if (e->d_type == DT_REG && is_rom(e->d_name)) {
      strncpy(arr[cnt].name, e->d_name, 255);
      snprintf(arr[cnt].path, 512, "%s/%s", cwd, e->d_name);
      printf("[%d] %s\n", cnt, arr[cnt].name);
      cnt++;
    }
  }
  closedir(d);

  if (!cnt) {
    puts("no *" ROM_SUFFIX);
    return 0;
  }

  // Read a whole line, then sscanf() it: this sidesteps scanf("%d")'s "leave
  // the offending char unconsumed" semantics that would otherwise spin forever
  // on malformed input.  (fflush(stdin) is UB and must be avoided.)
  int sel;
  char line[32];
  for (;;) {
    printf("\nselect index: ");
    if (fgets(line, sizeof line, stdin) == NULL)
      return 0; /* EOF: no valid selection possible */
    if (sscanf(line, "%d", &sel) == 1 && sel >= 0 && sel < cnt)
      break;
    puts("invalid selection!");
  }

  printf("selected: %s\n", arr[sel].path);

  FILE *fp = fopen(arr[sel].path, "rb");
  if (!fp) {
    perror("fopen\n");
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  size_t fsize = ftell(fp);
  if (size)
    *size = fsize;

  char *data = (char *)malloc(fsize);
  fseek(fp, 0, SEEK_SET);
  fread(data, fsize, 1, fp);
  fclose(fp);

  return data;
}

int main(int argc, char *argv[]) {
  char *data = select_rom(NULL);
  if (!data) {
    fprintf(stderr, "select_rom error.\n");
    return 1;
  }

  if (fce_load_rom(data) != 0) {
    fprintf(stderr, "Invalid or unsupported rom.\n");
    exit(1);
  }
  fce_init();
  fce_run();

  free(data);
  return 0;
}
