//
// Created by zhangjiantao on 2026/6/24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "disc.h"
#include "machw.h"
#include "rom.h"
#include "umac.h"

// #include "keymap_sdl.h"

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

static void *TCM_VRAM = NULL;

static uint8_t mac_to_rgb332_lut[256][8];

void init_mac_to_rgb332_lut(void) {
  for (int i = 0; i < 256; i++) {
    for (int bit = 0; bit < 8; bit++) {
      if (i & (0x80 >> bit)) {
        mac_to_rgb332_lut[i][bit] = 0x00;
      } else {
        mac_to_rgb332_lut[i][bit] = 0xFF;
      }
    }
  }
}

static void copy_fb(const uint8_t *fb_in) {
  uint64_t *vram = (uint64_t *)TCM_VRAM;

  const uint32_t row_bytes = DISP_WIDTH / 8;

  for (int y = 0; y < DISP_HEIGHT; y++) {
    const uint8_t *row_in = &fb_in[y * row_bytes];
    for (uint32_t x_byte = 0; x_byte < row_bytes; x_byte += 8) {

      uint8_t plo0 = row_in[x_byte + 0];
      uint8_t phi0 = row_in[x_byte + 1];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[plo0];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[phi0];

      uint8_t plo1 = row_in[x_byte + 2];
      uint8_t phi1 = row_in[x_byte + 3];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[plo1];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[phi1];

      uint8_t plo2 = row_in[x_byte + 4];
      uint8_t phi2 = row_in[x_byte + 5];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[plo2];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[phi2];

      uint8_t plo3 = row_in[x_byte + 6];
      uint8_t phi3 = row_in[x_byte + 7];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[plo3];
      *vram++ = *(uint64_t *)mac_to_rgb332_lut[phi3];
    }
  }
}

#include "keymap.h"

static int mouse_speed_x = 0;
static int mouse_speed_y = 0;
static int mouse_btn_state = 0;

static void event_handler(void) {
  TcmKeyboardEvent k;
  k.raw_value = tcm_syscall_read_keyboard();
  if (!k.raw_value) {
    if (mouse_speed_x != 0 || mouse_speed_y != 0) {
      umac_mouse(mouse_speed_x, mouse_speed_y, mouse_btn_state);
    }
    return;
  }

  uint32_t kcode = k.event.key_code;
  uint32_t press = k.event.is_press;

  if (press) {
    switch (kcode) {
    case TCM_K_UP:
      mouse_speed_y = 4;
      break;
    case TCM_K_DOWN:
      mouse_speed_y = -4;
      break;
    case TCM_K_LEFT:
      mouse_speed_x = -4;
      break;
    case TCM_K_RIGHT:
      mouse_speed_x = 4;
      break;
    case TCM_K_ENTER:
      mouse_btn_state = 1;
      umac_mouse(0, 0, 1);
      break;
    }
  } else {
    switch (kcode) {
    case TCM_K_UP:
    case TCM_K_DOWN:
      mouse_speed_y = 0;
      break;
    case TCM_K_LEFT:
    case TCM_K_RIGHT:
      mouse_speed_x = 0;
      break;
    case TCM_K_ENTER:
      mouse_btn_state = 0;
      umac_mouse(0, 0, 0);
      break;
    }
  }

  if (mouse_speed_x != 0 || mouse_speed_y != 0) {
    umac_mouse(mouse_speed_x, mouse_speed_y, mouse_btn_state);
  }
}

int main(int argc, char *argv[]) {
  TCM_VRAM = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, DISP_WIDTH);
  tcm_pixel_console_clear();

  init_mac_to_rgb332_lut();

  void *ram_base;
  void *rom_base;
  void *disc_base;
  int opt_disassemble = 1;

  FILE *dskfp = fopen("DEMO.dsk", "rb");
  if (dskfp == NULL) {
    printf("Error opening dsk file!");
    return 1;
  }

  fseek(dskfp, 0, SEEK_END);
  size_t disc_size = ftell(dskfp);
  disc_base = malloc(disc_size);
  fseek(dskfp, 0, SEEK_SET);
  fread(disc_base, disc_size, 1, dskfp);
  fclose(dskfp);

  printf("Disc mapped at %p, size %zu\n", (void *)disc_base, disc_size);

  disc_descr_t discs[DISC_NUM_DRIVES] = {0};
  discs[0].base = disc_base;
  discs[0].read_only = 0; /* See above */
  discs[0].size = disc_size;

  FILE *romfp = fopen("1986-03-4D1F8172-MacPlusv3.ROM", "rb");
  if (romfp == NULL) {
    printf("Error opening rom file!");
    return 1;
  }

  fseek(romfp, 0, SEEK_END);
  size_t rom_size = ftell(romfp);
  rom_base = malloc(rom_size);
  fseek(romfp, 0, SEEK_SET);
  fread(rom_base, rom_size, 1, romfp);
  fclose(romfp);

  if (rom_patch(rom_base)) {
    printf("Failed to patch ROM\n");
    return 1;
  }

  ram_base = malloc(RAM_SIZE);
  printf("RAM mapped at %p\n", (void *)ram_base);

  umac_init(ram_base, rom_base, discs);
  umac_opt_disassemble(opt_disassemble);

  int done = 0;
  uint64_t last_vsync = 0;
  uint64_t last_1hz = 0;
  do {
    struct timeval tv_now;

    event_handler();

    done |= umac_loop();

    gettimeofday(&tv_now, NULL);
    uint64_t now_usec = (tv_now.tv_sec * 1000000) + tv_now.tv_usec;

    /* Passage of time: */
    if ((now_usec - last_vsync) >= 16667) {
      umac_vsync_event();
      last_vsync = now_usec;
      copy_fb(ram_get_base() + umac_get_fb_offset());
    }
    if ((now_usec - last_1hz) >= 1000000) {
      umac_1hz_event();
      last_1hz = now_usec;
    }
  } while (!done);

  free(ram_base);
  free(rom_base);
  free(disc_base);

  return 0;
}
