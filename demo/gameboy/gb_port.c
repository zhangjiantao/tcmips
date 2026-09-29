//
// Created by zhangjiantao on 2026/6/1.
//

// https://wowroms.com/en/roms/nintendo-gameboy/download-super-mario-land-world/10202.html

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENABLE_SOUND 0
#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

uint8_t *rom_data = NULL;
size_t rom_data_size = 0;

#define MAX_ROM 32
#define ROM_SUFFIX ".gb"

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

static uint32_t *SF_FRAMEBUFFER = NULL;

const uint8_t gb_palette1[4] = {
    0xDE, // 0: 亮绿 (Game Boy 经典背景色)
    0x99, // 1: 浅绿
    0x6D, // 2: 深绿
    0x28  // 3: 暗绿 (马里奥轮廓与文字)
};

const uint8_t gb_palette2[4] = {
    0xFF, // 0: 暖白 (111 111 11) -> 完美的背景色
    0xB6, // 1: 淡灰 (101 101 10)
    0x6D, // 2: 深灰 (011 011 01)
    0x00  // 3: 纯黑 (000 000 00) -> 角色与文字轮廓
};

const uint8_t gb_palette3[4] = {
    0xDF, // 0: 浅天蓝 (110 111 11)
    0x9A, // 1: 迷雾灰 (100 110 10)
    0x48, // 2: 巧克力 (010 010 00)
    0x00  // 3: 纯黑   (000 000 00)
};

const uint8_t gb_palette4[4] = {
    0xFB, // 0: 草莓淡粉 (111 110 11) -> 背景
    0xF1, // 1: 樱花粉红 (111 100 01)
    0x80, // 2: 胭脂深红 (100 000 00)
    0x20  // 3: 黑樱桃紫 (001 000 00) -> 核心文字
};

uint8_t gb_rom_read(struct gb_s *gb, const uint32_t addr) {
  if (!rom_data) {
    rom_data = (uint8_t *)select_rom(&rom_data_size);
    if (!rom_data) {
      printf("No rom *.gb found!\n");
      exit(1);
    }

    SF_FRAMEBUFFER = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 192);
    tcm_pixel_console_clear();
  }

  if (addr >= rom_data_size) {
    printf("Error: addr %x rom len %x\n", addr, rom_data_size);
    return 0xFF;
  }
  return rom_data[addr];
}

uint8_t gb_cart_ram_array[32768] = {0};

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
  if (addr < 32768) {
    return gb_cart_ram_array[addr];
  }
  return 0xFF; // 超出正常 RAM 范围返回空总线电平
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr,
                       const uint8_t val) {
  if (addr < 32768) {
    gb_cart_ram_array[addr] = val;
  }
}

void gb_error(struct gb_s *gb, const enum gb_error_e gb_err,
              const uint16_t val) {
  printf("Error: gb err %x\n", val);
  exit(val);
}

// #define GB_PLT

#ifdef GB_PLT

__attribute__((constructor)) void init_palette(void) {
  tcm_syscall_palette_init(0, *(uint32_t *)gb_palette1);
}

void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint8_t line) {
  uint32_t fb_offset = line * 48 + 4;

  int p_idx = 0;
  for (int x = 0; x < 40; x += 2) {
    uint32_t packed_words_low = *(uint32_t *)(pixels + p_idx);
    uint32_t packed_words_high = *(uint32_t *)(pixels + p_idx + 4);
    packed_words_low &= 0x03030303;
    packed_words_high &= 0x03030303;
    tcm_syscall_palette_set_offset((uint32_t)(SF_FRAMEBUFFER + fb_offset + x));
    tcm_syscall_palette_upload(packed_words_low, packed_words_high);
    p_idx += 8;
  }
}

#else

void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint8_t line) {
  uint32_t fb_offset = line * 48 + 4;

  static const uint8_t *gb_palette = NULL;
  if (gb_palette == NULL) {
    switch (tcm_syscall_get_timestamp() % 4) {
    case 0: {
      gb_palette = gb_palette1;
      break;
    }
    case 1: {
      gb_palette = gb_palette2;
      break;
    }
    case 2: {
      gb_palette = gb_palette3;
      break;
    }
    case 3: {
      gb_palette = gb_palette4;
      break;
    }
    }
  }

  int p_idx = 0;
  for (int x = 0; x < 40; x += 2) {
    uint8_t p0 = gb_palette[pixels[p_idx + 0] & 3];
    uint8_t p1 = gb_palette[pixels[p_idx + 1] & 3];
    uint8_t p2 = gb_palette[pixels[p_idx + 2] & 3];
    uint8_t p3 = gb_palette[pixels[p_idx + 3] & 3];

    uint8_t p4 = gb_palette[pixels[p_idx + 4] & 3];
    uint8_t p5 = gb_palette[pixels[p_idx + 5] & 3];
    uint8_t p6 = gb_palette[pixels[p_idx + 6] & 3];
    uint8_t p7 = gb_palette[pixels[p_idx + 7] & 3];

    uint32_t packed_words_low = (p3 << 24) | (p2 << 16) | (p1 << 8) | p0;
    uint32_t packed_words_high = (p7 << 24) | (p6 << 16) | (p5 << 8) | p4;

    SF_FRAMEBUFFER[fb_offset + x] = packed_words_low;
    SF_FRAMEBUFFER[fb_offset + x + 1] = packed_words_high;

    p_idx += 8;
  }
}

#endif

void poll_hardware_input(struct gb_s *gb) {
  while (1) {
    TcmKeyboardEvent k;
    k.raw_value = tcm_syscall_read_keyboard();

    if (k.raw_value == 0)
      break;

    bool press = k.event.is_press;
    uint8_t kcode = k.event.key_code;
    uint8_t mask = 0;

    switch (kcode) {
    case 'z':
      mask = JOYPAD_A;
      break;
    case 'x':
      mask = JOYPAD_B;
      break;
    case TCM_K_LSHIFT:
    case TCM_K_RSHIFT:
      mask = JOYPAD_SELECT;
      break;
    case TCM_K_ENTER:
      mask = JOYPAD_START;
      break;

    case TCM_K_RIGHT:
      mask = JOYPAD_RIGHT;
      break;
    case TCM_K_LEFT:
      mask = JOYPAD_LEFT;
      break;
    case TCM_K_UP:
      mask = JOYPAD_UP;
      break;
    case TCM_K_DOWN:
      mask = JOYPAD_DOWN;
      break;

    default:
      continue;
    }

    if (press) {
      gb->direct.joypad &= ~mask;
    } else {
      gb->direct.joypad |= mask;
    }
  }
}
