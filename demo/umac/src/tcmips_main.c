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

#include "dev/keyboard.h"

#include <dev/console.h>
#include <dev/syscall.h>

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
  uint64_t *vram = (uint64_t *)TCM_VRAM_PIXEL_ADDR;

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

#include "../resources/umac-disc.h"
#include "../resources/umac-rom.h"

#include "keymap.h"

static int mouse_speed_x = 0;
static int mouse_speed_y = 0;
static int mouse_btn_state = 0;

static void event_handler(void) {
  uint32_t kev = tcm_syscall_read_keyboard();
  if (!kev) {
    if (mouse_speed_x != 0 || mouse_speed_y != 0) {
      umac_mouse(mouse_speed_x, mouse_speed_y, mouse_btn_state);
    }
    return;
  }

  uint32_t kcode = __TCM_KEYBOARD_KCODE(kev);
  uint32_t press = __TCM_KEYBOARD_PRESS(kev);

  if (press) {
    switch (kcode) {
    case __TCM_KEY_CODE_UP:
      mouse_speed_y = 4;
      break;
    case __TCM_KEY_CODE_DOWN:
      mouse_speed_y = -4;
      break;
    case __TCM_KEY_CODE_LEFT:
      mouse_speed_x = -4;
      break;
    case __TCM_KEY_CODE_RIGHT:
      mouse_speed_x = 4;
      break;
    case __TCM_KEY_CODE_ENTER:
      mouse_btn_state = 1;
      umac_mouse(0, 0, 1);
      break;
    }
  } else {
    switch (kcode) {
    case __TCM_KEY_CODE_UP:
    case __TCM_KEY_CODE_DOWN:
      mouse_speed_y = 0;
      break;
    case __TCM_KEY_CODE_LEFT:
    case __TCM_KEY_CODE_RIGHT:
      mouse_speed_x = 0;
      break;
    case __TCM_KEY_CODE_ENTER:
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
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, DISP_WIDTH);
  tcm_pixel_console_clear();

  init_mac_to_rgb332_lut();

  void *ram_base;
  void *rom_base;
  void *disc_base;
  int opt_disassemble = 1;

  rom_base = __1986_03_4D1F8172_MacPlusv3_ROM;
  if (rom_patch(rom_base)) {
    printf("Failed to patch ROM\n");
    return 1;
  }

  ram_base = malloc(RAM_SIZE);
  printf("RAM mapped at %p\n", (void *)ram_base);

  disc_descr_t discs[DISC_NUM_DRIVES] = {0};

  disc_base = DEMO_dsk;
  size_t disc_size = DEMO_dsk_len;
  printf("Disc mapped at %p, size %zu\n", (void *)disc_base, disc_size);

  discs[0].base = disc_base;
  discs[0].read_only = 0; /* See above */
  discs[0].size = disc_size;

  umac_init(ram_base, rom_base, discs);
  umac_opt_disassemble(opt_disassemble);

  int done = 0;
  uint64_t last_vsync = 0;
  uint64_t last_1hz = 0;
  do {
    struct timeval tv_now;

    // event
    // if (SDL_PollEvent(&event)) {
    //   umac_kbd_event(c, (event.type == SDL_KEYDOWN));
    //   umac_mouse(mousex, mousey, mouse_button);
    // }

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

  return 0;
}
