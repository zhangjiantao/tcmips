#include "dev/keyboard.h"
#include "wl_def.h"

#include <dev/console.h>
#include <dev/syscall.h>

#define FIXME abort

void TimerInit(void) {}

void Quit(const char *error) {
  if (error) {
    printf("\n!!! GAME FATAL QUIT !!!\n Reason: %s\n", error);
  } else {
    printf("\n!!! GAME QUIT TO DOS ATTEMPTED !!!\n");
  }

  exit(error ? 1 : 0);
}

/* Input.  */

void INL_Update() {
  TcmKeyboardPacket k;
  k.raw_value = tcm_syscall_read_keyboard();
  bool press = k.event.is_press;
  uint32_t scancode = k.event.key_code;

  while (scancode != 0) {
    switch (scancode) {
    case 0x20:
      scancode = sc_Space;
      break;
    case __TCM_KEY_CODE_CTRL:
      scancode = sc_Control;
      break;
    case __TCM_KEY_CODE_UP:
      scancode = sc_UpArrow;
      break;
    case __TCM_KEY_CODE_DOWN:
      scancode = sc_DownArrow;
      break;
    case __TCM_KEY_CODE_LEFT:
      scancode = sc_LeftArrow;
      break;
    case __TCM_KEY_CODE_RIGHT:
      scancode = sc_RightArrow;
      break;
    default:
      scancode = 0;
      break;
    }
    if (scancode == 0)
      continue;
    keyboard_handler(scancode, press);

    k.raw_value = tcm_syscall_read_keyboard();
    press = k.event.is_press;
    scancode = k.event.key_code;
  }

#ifdef INTEGRATOR
  int scancode;
  while (kmi0[1] & 0x10) {
    int press = 1;
    scancode = kmi0[2];
#if 0
	printf ("scancode %x\n", scancode);
#else
    if (scancode == 0xe0) {
      scancode = kmi0[2] | 0x100;
    }
    if ((scancode & 0xff) == 0xf0) {
      press = 0;
      scancode = (scancode & 0x100) | kmi0[2];
    }
    switch (scancode) {
    case 0x29:
      scancode = sc_Space;
      break;
    case 0x14:
      scancode = sc_Control;
      break;
    case 0x175:
      scancode = sc_UpArrow;
      break;
    case 0x172:
      scancode = sc_DownArrow;
      break;
    case 0x16b:
      scancode = sc_LeftArrow;
      break;
    case 0x174:
      scancode = sc_RightArrow;
      break;
    default:
      scancode = 0;
      break;
    }
    if (scancode == 0)
      continue;
    keyboard_handler(scancode, press);
#endif
  }
#endif
#ifdef LUMINARY
  int buttons = 0;
  static int oldbuttons = 0;
  int i;
  static const byte scancodes[5] = {sc_UpArrow, sc_DownArrow, sc_LeftArrow,
                                    sc_RightArrow, sc_Space};

  buttons = read_buttons();
  buttons ^= oldbuttons;
  oldbuttons ^= buttons;
  for (i = 0; i < 5; i++) {
    int mask = 1 << i;
    if (buttons & mask) {
      keyboard_handler(scancodes[i], (oldbuttons & mask) != 0);
    }
  }

#endif
}

/* Graphics bits.  */

byte framebuffer[128 * 64 / 2];

static uint32_t tc_palette16[16];

void VL_Startup() {
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 128);
  tcm_pixel_console_clear();

  for (int i = 0; i < 256; i++) {
    uint8_t target_16_idx = pal4bit[i] & 0x0F;

    uint8_t r = gamepal[i * 3] << 2;
    uint8_t g = gamepal[i * 3 + 1] << 2;
    uint8_t b = gamepal[i * 3 + 2] << 2;

    tc_palette16[target_16_idx] = (b << 16) | (g << 8) | r;
  }
}

void VW_UpdateScreen() {
  volatile uint32_t *vram = (volatile uint32_t *)TCM_VRAM_PIXEL_ADDR;

  for (int y = 0; y < 64; y++) {
    volatile uint32_t *vram_row1 = &vram[(y * 2) * 256];
    volatile uint32_t *vram_row2 = &vram[(y * 2 + 1) * 256];

    for (int x_byte = 0; x_byte < 64; x_byte++) {
      uint8_t packed_byte = framebuffer[y * 64 + x_byte];

      uint8_t pixel_left = (packed_byte >> 4) & 0x0F;
      uint8_t pixel_right = packed_byte & 0x0F;

      uint32_t rgb_left = tc_palette16[pixel_left];
      uint32_t rgb_right = tc_palette16[pixel_right];

      int p_idx = x_byte * 4;

      vram_row1[p_idx] = rgb_left;
      vram_row1[p_idx + 1] = rgb_left;
      vram_row2[p_idx] = rgb_left;
      vram_row2[p_idx + 1] = rgb_left;

      vram_row1[p_idx + 2] = rgb_right;
      vram_row1[p_idx + 3] = rgb_right;
      vram_row2[p_idx + 2] = rgb_right;
      vram_row2[p_idx + 3] = rgb_right;
    }
  }
}

static void sys_init() {}

int main(int argc, char **argv) {
  // #ifndef LUMINARY
  //     vwidth = 128;
  //     vheight = 96;
  // #endif
  sys_init();
  TimerInit();
  return WolfMain(argc, argv);
}
