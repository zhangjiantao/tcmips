// "headless" tiny386
// for SDL port, see `sdl/main.c`

#include "pc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// platform HAL implementation
#include <time.h>

uint32_t get_uticks() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((uint32_t)ts.tv_sec * 1000000 + (uint32_t)ts.tv_nsec / 1000);
}

void *bigmalloc(size_t size) { return malloc(size); }

int load_rom(void *phys_mem, const char *file, uword addr, int backward) {
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) {
    fprintf(stderr, "load_rom: open %s failed: %s\n", file, strerror(errno));
    abort();
  }

  fseek(fp, 0, SEEK_END);
  int len = ftell(fp);
  fprintf(stderr, "load_rom: %s, len %d\n", file, len);
  rewind(fp);
  if (backward)
    fread(phys_mem + addr - len, 1, len, fp);
  else
    fread(phys_mem + addr, 1, len, fp);
  fclose(fp);
  return len;
}

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#define TCM_FRAMEBUFFER ((uint32_t *)TCM_VRAM_PIXEL_ADDR)
#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 480

static void redraw(void *opaque, int x, int y, int w, int h) {
  memcpy(TCM_FRAMEBUFFER, opaque, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
  return;
  uint32_t *virtual_fb = (uint32_t *)opaque;

  for (int row = y; row < y + h; ++row) {
    uint32_t *src = virtual_fb + (row * 720) + x;
    uint32_t *dst = TCM_FRAMEBUFFER + (row * SCREEN_WIDTH) + x;

#ifdef __tcm__
    for (int col = 0; col < w; col += 8) {
      dst[col] = src[col];
      dst[col + 1] = src[col + 1];
      dst[col + 2] = src[col + 2];
      dst[col + 3] = src[col + 3];
      dst[col + 4] = src[col + 4];
      dst[col + 5] = src[col + 5];
      dst[col + 6] = src[col + 6];
      dst[col + 7] = src[col + 7];
    }
#else
    for (int col = 0; col < w; ++col) {
      uint32_t pixel32 = src[col];
      dst[col] = __builtin_bswap32(pixel32) >> 8;
    }
#endif
  }
}

#ifdef TCM_TINY386_KBD
static uint32_t tcm_trans_keycode(uint32_t keycode) {
  switch (keycode) {
  case __TCM_KEY_CODE_UP:
    return 0x67;
  case __TCM_KEY_CODE_DOWN:
    return 0x6c;
  case __TCM_KEY_CODE_LEFT:
    return 0x69;
  case __TCM_KEY_CODE_RIGHT:
    return 0x6a;
  case __TCM_KEY_CODE_DEL:
    return 0x6f;
  case __TCM_KEY_CODE_ENTER:
    return 0x60;
  case __TCM_KEY_CODE_CTRL:
    return 0x61;
  default:
    if (keycode == 0)
      return 0;
  }

  if (keycode < 127 + 9) {
    keycode -= 8;
  } else {
    keycode = 0;
  }
  return keycode;
}

static void tcm_handle_key_event(PC *pc) {
  uint32_t keycode, keypress;

  uint32_t k = tcm_syscall_read_keyboard();
  if (k == 0)
    return;

  keypress = __TCM_KEYBOARD_PRESS(k);
  keycode = __TCM_KEYBOARD_KCODE(k);
  keycode = tcm_trans_keycode(keycode);

  if (keycode)
    ps2_put_keycode(pc->kbd, (int)keypress, (int)keycode);
}
#endif

static PCConfig conf;
int main(int argc, char *argv[]) {
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_WIDTH);
  tcm_pixel_console_clear();

  memset(&conf, 0, sizeof(conf));
  conf.mem_size = 16 * 1024 * 1024;
  conf.vga_mem_size = 768 * 1024;
  conf.width = 720;
  conf.height = 480;
  conf.cpu_gen = 4;
  conf.fpu = 0;

  conf.enable_serial = 1;

  conf.bios = "bios.bin";
  conf.vga_bios = "vgabios.bin";

#ifdef TCM_TINY386_WIN95
  conf.disks[0] = "micro95.img";
  conf.iscd[0] = 0;
#endif

#ifdef TCM_TINY386_FREEDOS
  conf.fdd[0] = "freedos.img";
#endif

  conf.fill_cmos = 1;

  uint8_t *fb = bigmalloc(conf.width * conf.height * 4);
  PC *pc = pc_new(redraw, fb, fb, &conf);
  load_bios_and_reset(pc);

  pc->boot_start_time = get_uticks();
  for (; pc->shutdown_state != 8;) {
    pc_step(pc);
#ifdef TCM_TINY386_KBD
    tcm_handle_key_event(pc);
#endif
    pc_vga_step(pc);
  }
  return 0;
}
