/*
 * Host Layer — Implementation
 *
 * What lives here:
 *   • MinifB window lifecycle (open / close / resize)
 *   • VRAM → screen bilinear upscaling (32-bit / 8-bit palette)
 *   • Frame-pacing render loop (main thread) with guest exit-code capture
 *     from BREAK(0x3ff) via GPR[5]
 *   • Keyboard mapping macro — replaces the old hand-written switch()
 *   • `-stdin=<path>` argument handling (guest semihost stdin bridge)
 */

#include "host.h"
#include "cpu_debug.h"
#include "cpu_fast.h"
#include "cpu_jit.h"
#include "emulator_exit.h"
#include "memory.h"
#include "peripheral.h"
#include "platform.h"

#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minifb/include/MiniFB.h"

typedef struct {
  const cpu_interpreter_t *interp;
  bool started;
  bool finished;
  uint64_t last_count;
  uint64_t acc_ns; /* accumulated nanoseconds since last title update */
  uint64_t uptime_ns;
  PlatformTimer
      uptime_timer; /* resets every call; accumulated into uptime_ns */
  uint32_t exit_code;
} cpu_status_t;

static volatile cpu_status_t g_cpu BSS_SECTION;

/* ==================================================================== */
/*  Display constants                                                   */
/* ==================================================================== */

#ifdef __APPLE__
#define SCREEN_W (640 * 1 * 2)
#define SCREEN_H (480 * 1)
#define OUT_W (640 * 1)
#define OUT_H (480 * 1)
#define OUT_X0 0
#define OUT_Y0 0
#define OUT_X1 (640 * 1)
#define OUT_Y1 0
#else
#define SCREEN_W (640 * 2 * 2)
#define SCREEN_H (480 * 2)
#define OUT_W (640 * 2)
#define OUT_H (480 * 2)
#define OUT_X0 0
#define OUT_Y0 0
#define OUT_X1 (640 * 2)
#define OUT_Y1 0
#endif

static uint32_t g_minifb_screen[SCREEN_W * SCREEN_H]
    __attribute__((aligned(32))) BSS_SECTION;

/* ==================================================================== */
/*  Host state                                                          */
/* ==================================================================== */

static struct mfb_window *g_window = NULL;
static FILE *g_stdin_fp = NULL; /* semihost stdin backing */

/* Running flag — observed by both the CPU thread and the render thread.
   Starts true and is cleared by BREAK, window-close, or host_shutdown(). */
static volatile bool g_emu_running = true;

/* Set by the keyboard callback only when the guest has already terminated
   (g_cpu_finished).  The post-guest hold loop reads this to know when the
   user has dismissed the window. */
static volatile bool g_dismiss = false;

bool host_is_running(void) { return g_emu_running; }

void host_stop_running(void) { g_emu_running = false; }

/* ==================================================================== */
/*  Bilinear up-scaling helpers (VRAM → screen)                          */
/* ==================================================================== */

/*
 * MiniFB desktop/iOS/web backends expect BGRA byte order in memory (B at the
 * lowest address).  On little-endian that is the packed uint32 value
 * 0x00RRGGBB, with A in the high byte and R in bits 16..23, G in 8..15, B in
 * 0..7.
 */
static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Bilinear interpolation of a single channel followed by [0,255] clamping.
   Keeps the original signed-arithmetic behavior of resize_to_xy_*. */
static inline uint8_t lerp_channel(int c00, int c10, int c01, int c11, int fx,
                                   int fy) {
  int c0 = c00 + (((c10 - c00) * fx) >> 8);
  int c1 = c01 + (((c11 - c01) * fx) >> 8);
  int v = c0 + (((c1 - c0) * fy) >> 8);
  if (v < 0)
    v = 0;
  if (v > 255)
    v = 255;
  return (uint8_t)v;
}

/* RGB332 → RGB888 expansion tables (truncated x * 255 / n, matching the old
   integer division behaviour exactly so output pixels stay bit-identical). */
static const uint8_t r332_to_888[8] = {0, 36, 72, 109, 145, 182, 218, 255};
static const uint8_t g332_to_888[8] = {0, 36, 72, 109, 145, 182, 218, 255};
static const uint8_t b332_to_888[4] = {0, 85, 170, 255};

/** Scale a 32-bit RGBA8888 pixel VRAM tile into the destination buffer. */
static void scale_32(const uint8_t *_src, int src_w, int src_h, uint32_t *dst,
                     int dst_w, int dst_h, int dst_x, int dst_y) {
  uint32_t *src = (uint32_t *)_src;

  if (!src || !dst || src_w <= 0 || src_h <= 0)
    return;
  if (OUT_W <= 1 || OUT_H <= 1)
    return;

  int dst_x0 = dst_x < 0 ? 0 : dst_x;
  int dst_y0 = dst_y < 0 ? 0 : dst_y;
  int dst_x1 = dst_x + OUT_W;
  if (dst_x1 > dst_w)
    dst_x1 = dst_w;
  int dst_y1 = dst_y + OUT_H;
  if (dst_y1 > dst_h)
    dst_y1 = dst_h;
  if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1)
    return;

  uint32_t step_x = ((uint32_t)(src_w - 1) << 16) / (OUT_W - 1);
  uint32_t step_y = ((uint32_t)(src_h - 1) << 16) / (OUT_H - 1);

  for (int real_y = dst_y0; real_y < dst_y1; ++real_y) {
    int dy = real_y - dst_y;
    uint32_t sy = (uint32_t)dy * step_y;
    int y0 = sy >> 16;
    int y1 = y0 + 1;
    if (y1 >= src_h)
      y1 = src_h - 1;
    int fy = (sy >> 8) & 0xFF;

    for (int real_x = dst_x0; real_x < dst_x1; ++real_x) {
      int dx = real_x - dst_x;
      uint32_t sx = (uint32_t)dx * step_x;
      int x0 = sx >> 16;
      int x1 = x0 + 1;
      if (x1 >= src_w)
        x1 = src_w - 1;
      int fx = (sx >> 8) & 0xFF;

      uint32_t p00 = src[y0 * src_w + x0];
      uint32_t p10 = src[y0 * src_w + x1];
      uint32_t p01 = src[y1 * src_w + x0];
      uint32_t p11 = src[y1 * src_w + x1];

      uint8_t r = lerp_channel((p00 >> 0) & 0xFF, (p10 >> 0) & 0xFF,
                               (p01 >> 0) & 0xFF, (p11 >> 0) & 0xFF, fx, fy);
      uint8_t g = lerp_channel((p00 >> 8) & 0xFF, (p10 >> 8) & 0xFF,
                               (p01 >> 8) & 0xFF, (p11 >> 8) & 0xFF, fx, fy);
      uint8_t b = lerp_channel((p00 >> 16) & 0xFF, (p10 >> 16) & 0xFF,
                               (p01 >> 16) & 0xFF, (p11 >> 16) & 0xFF, fx, fy);

      dst[real_y * dst_w + real_x] = pack_rgb(r, g, b);
    }
  }
}

/**
 * Scales a 32-bit RGBA8888 pixel VRAM tile into the destination buffer
 * using Nearest-Neighbor interpolation to preserve sharp pixel-art edges.
 */
static void scale_32_nearest(const uint8_t *_src, int src_w, int src_h,
                             uint32_t *dst, int dst_w, int dst_h, int dst_x,
                             int dst_y) {
  uint32_t *src = (uint32_t *)_src;

  // Guard clause for invalid pointers or empty dimensions
  if (!src || !dst || src_w <= 0 || src_h <= 0)
    return;
  if (OUT_W <= 0 || OUT_H <= 0)
    return;

  // Calculate the clipping boundaries within the destination buffer
  int dst_x0 = dst_x < 0 ? 0 : dst_x;
  int dst_y0 = dst_y < 0 ? 0 : dst_y;
  int dst_x1 = dst_x + OUT_W;
  if (dst_x1 > dst_w)
    dst_x1 = dst_w;
  int dst_y1 = dst_y + OUT_H;
  if (dst_y1 > dst_h)
    dst_y1 = dst_h;

  // If the rendering area is completely clipped out, exit early
  if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1)
    return;

  // Calculate scaling steps using 16.16 fixed-point math for high performance
  uint32_t step_x = ((uint32_t)src_w << 16) / OUT_W;
  uint32_t step_y = ((uint32_t)src_h << 16) / OUT_H;

  // Iterate through each row of the destination bounding box
  for (int real_y = dst_y0; real_y < dst_y1; ++real_y) {
    int dy = real_y - dst_y;
    uint32_t sy = (uint32_t)dy * step_y;

    // Convert fixed-point back to integer to get the nearest source Y
    // coordinate
    int src_real_y = sy >> 16;
    if (src_real_y >= src_h)
      src_real_y = src_h - 1;

    // Iterate through each pixel of the current row
    for (int real_x = dst_x0; real_x < dst_x1; ++real_x) {
      int dx = real_x - dst_x;
      uint32_t sx = (uint32_t)dx * step_x;

      // Convert fixed-point back to integer to get the nearest source X
      // coordinate
      int src_real_x = sx >> 16;
      if (src_real_x >= src_w)
        src_real_x = src_w - 1;

      // Directly copy the source color without any blending to maintain crisp
      // edges
      dst[real_y * dst_w + real_x] =
          __builtin_bswap32(src[src_real_y * src_w + src_real_x]) >> 8;
    }
  }
}

/** Scale an 8-bit RGB332 palette VRAM tile into the destination buffer. */
static void scale_8(const uint8_t *src, int src_w, int src_h, uint32_t *dst,
                    int dst_w, int dst_h, int dst_x, int dst_y) {
  if (!src || !dst || src_w <= 0 || src_h <= 0)
    return;
  if (OUT_W <= 1 || OUT_H <= 1)
    return;

  int dst_x0 = dst_x < 0 ? 0 : dst_x;
  int dst_y0 = dst_y < 0 ? 0 : dst_y;
  int dst_x1 = dst_x + OUT_W;
  if (dst_x1 > dst_w)
    dst_x1 = dst_w;
  int dst_y1 = dst_y + OUT_H;
  if (dst_y1 > dst_h)
    dst_y1 = dst_h;
  if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1)
    return;

  uint32_t step_x = ((uint32_t)(src_w - 1) << 16) / (OUT_W - 1);
  uint32_t step_y = ((uint32_t)(src_h - 1) << 16) / (OUT_H - 1);

  for (int real_y = dst_y0; real_y < dst_y1; ++real_y) {
    int dy = real_y - dst_y;
    uint32_t sy = (uint32_t)dy * step_y;
    int y0 = sy >> 16;
    int y1 = y0 + 1;
    if (y1 >= src_h)
      y1 = src_h - 1;
    int fy = (sy >> 8) & 0xFF;

    for (int real_x = dst_x0; real_x < dst_x1; ++real_x) {
      int dx = real_x - dst_x;
      uint32_t sx = (uint32_t)dx * step_x;
      int x0 = sx >> 16;
      int x1 = x0 + 1;
      if (x1 >= src_w)
        x1 = src_w - 1;
      int fx = (sx >> 8) & 0xFF;

      uint8_t p00 = src[y0 * src_w + x0];
      uint8_t p10 = src[y0 * src_w + x1];
      uint8_t p01 = src[y1 * src_w + x0];
      uint8_t p11 = src[y1 * src_w + x1];

      uint8_t r00 = r332_to_888[(p00 >> 5) & 0x07];
      uint8_t r10 = r332_to_888[(p10 >> 5) & 0x07];
      uint8_t r01 = r332_to_888[(p01 >> 5) & 0x07];
      uint8_t r11 = r332_to_888[(p11 >> 5) & 0x07];

      uint8_t g00 = g332_to_888[(p00 >> 2) & 0x07];
      uint8_t g10 = g332_to_888[(p10 >> 2) & 0x07];
      uint8_t g01 = g332_to_888[(p01 >> 2) & 0x07];
      uint8_t g11 = g332_to_888[(p11 >> 2) & 0x07];

      uint8_t b00 = b332_to_888[p00 & 0x03];
      uint8_t b10 = b332_to_888[p10 & 0x03];
      uint8_t b01 = b332_to_888[p01 & 0x03];
      uint8_t b11 = b332_to_888[p11 & 0x03];

      uint8_t r = lerp_channel(r00, r10, r01, r11, fx, fy);
      uint8_t g = lerp_channel(g00, g10, g01, g11, fx, fy);
      uint8_t b = lerp_channel(b00, b10, b01, b11, fx, fy);

      dst[real_y * dst_w + real_x] = pack_rgb(r, g, b);
    }
  }
}

/**
 * Scales an 8-bit RGB332 pixel VRAM tile into a 32-bit destination buffer
 * using pure Nearest-Neighbor truncation to preserve classic sharp pixel edges.
 */
static void scale_8_nearest(const uint8_t *src, int src_w, int src_h,
                            uint32_t *dst, int dst_w, int dst_h, int dst_x,
                            int dst_y) {
  // Guard clause for invalid pointers or empty dimensions
  if (!src || !dst || src_w <= 0 || src_h <= 0)
    return;
  if (OUT_W <= 0 || OUT_H <= 0)
    return;

  // Calculate the clipping boundaries within the destination buffer
  int dst_x0 = dst_x < 0 ? 0 : dst_x;
  int dst_y0 = dst_y < 0 ? 0 : dst_y;
  int dst_x1 = dst_x + OUT_W;
  if (dst_x1 > dst_w)
    dst_x1 = dst_w;
  int dst_y1 = dst_y + OUT_H;
  if (dst_y1 > dst_h)
    dst_y1 = dst_h;

  // If the rendering area is completely clipped out, exit early
  if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1)
    return;

  // Calculate standard scaling step via 16.16 fixed-point math
  uint32_t step_x = ((uint32_t)src_w << 16) / OUT_W;
  uint32_t step_y = ((uint32_t)src_h << 16) / OUT_H;

  // Iterate through each row of the destination bounding box
  for (int real_y = dst_y0; real_y < dst_y1; ++real_y) {
    int dy = real_y - dst_y;
    // Pure fixed-point truncation mapping
    uint32_t sy = (uint32_t)dy * step_y;
    int src_real_y = sy >> 16;

    // Fast bounds clamping
    if (src_real_y >= src_h)
      src_real_y = src_h - 1;

    for (int real_x = dst_x0; real_x < dst_x1; ++real_x) {
      int dx = real_x - dst_x;
      // Pure fixed-point truncation mapping
      uint32_t sx = (uint32_t)dx * step_x;
      int src_real_x = sx >> 16;

      if (src_real_x >= src_w)
        src_real_x = src_w - 1;

      // Fetch the 8-bit RGB332 pixel value
      uint8_t p = src[src_real_y * src_w + src_real_x];

      // Convert RGB332 to RGB888 components using the lookup tables
      uint8_t r = r332_to_888[(p >> 5) & 0x07];
      uint8_t g = g332_to_888[(p >> 2) & 0x07];
      uint8_t b = b332_to_888[p & 0x03];

      // Pack and write the crisp pixel into the destination buffer
      dst[real_y * dst_w + real_x] = pack_rgb(r, g, b);
    }
  }
}

typedef void (*scale_fn_t)(const uint8_t *src, int src_w, int src_h,
                           uint32_t *dst, int dst_w, int dst_h, int dst_x,
                           int dst_y);

/* ==================================================================== */
/*  Keyboard mapping — macro-driven lookup table                        */
/*
 *  TCM_KBD_MAP  — single source of truth for every MinifB → TCMIPS
 *                 virtual key mapping.  Adding one line extends the
 *                 keyboard without touching any callback code.
 *
 *  DEFINE_KBD_CB(fn)                  — generates a Minifb keyboard
 *                 callback function using kbd_translate() plus a
 *                 generic fallback (space → 0x20, A-Z → +0x20).
 */

/* ── Map: one line per key ────────────────────────────────────────── */

#define TCM_KBD_MAP                                                            \
  X(MFB_KB_KEY_UP, TCM_K_UP)                                                   \
  X(MFB_KB_KEY_DOWN, TCM_K_DOWN)                                               \
  X(MFB_KB_KEY_LEFT, TCM_K_LEFT)                                               \
  X(MFB_KB_KEY_RIGHT, TCM_K_RIGHT)                                             \
  X(MFB_KB_KEY_LEFT_CONTROL, TCM_K_LCTRL)                                      \
  X(MFB_KB_KEY_RIGHT_CONTROL, TCM_K_RCTRL)                                     \
  X(MFB_KB_KEY_LEFT_ALT, TCM_K_LALT)                                           \
  X(MFB_KB_KEY_RIGHT_ALT, TCM_K_RALT)                                          \
  X(MFB_KB_KEY_LEFT_SHIFT, TCM_K_LSHIFT)                                       \
  X(MFB_KB_KEY_RIGHT_SHIFT, TCM_K_RSHIFT)                                      \
  X(MFB_KB_KEY_ENTER, TCM_K_ENTER)                                             \
  X(MFB_KB_KEY_BACKSPACE, TCM_K_BACKSPACE)                                     \
  X(MFB_KB_KEY_ESCAPE, TCM_K_ESC)                                              \
  X(MFB_KB_KEY_PAGE_UP, TCM_K_PAGEUP)                                          \
  X(MFB_KB_KEY_PAGE_DOWN, TCM_K_PAGEDOWN)                                      \
  X(MFB_KB_KEY_TAB, TCM_K_TAB)                                                 \
  X(MFB_KB_KEY_HOME, TCM_K_HOME)                                               \
  X(MFB_KB_KEY_END, TCM_K_END)                                                 \
  X(MFB_KB_KEY_INSERT, TCM_K_INSERT)                                           \
  X(MFB_KB_KEY_CAPS_LOCK, TCM_K_CAPSLOCK)                                      \
  X(MFB_KB_KEY_DELETE, TCM_K_DEL)

typedef struct {
  mfb_key mfb;
  uint16_t tcm;
} KbdMapEntry;

static const KbdMapEntry kbd_map[] = {
#define X(mfb_key, tcm_code) {mfb_key, tcm_code},
    TCM_KBD_MAP
#undef X
};
static const size_t kbd_map_cnt = sizeof(kbd_map) / sizeof(kbd_map[0]);

/** Translate a MinifB key code into a TCMIPS virtual key code.
 *  Returns 0 when the key is not in the map (falls through to generic
 *  handler in DEFINE_KBD_CB). */
static inline uint16_t kbd_translate(mfb_key key) {
  for (size_t i = 0; i < kbd_map_cnt; i++)
    if (kbd_map[i].mfb == key)
      return kbd_map[i].tcm;

  return 0;
}

static void mfb_keyboard_fn(struct mfb_window *w, mfb_key key, mfb_key_mod mod,
                            bool pressed) {
  TcmKeyboardEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.event.not_empty = 1;
  ev.event.is_press = (uint32_t)pressed;
  ev.event.key_code = kbd_translate(key);

  if (!ev.event.key_code) {
    /* Bare printable keys that were not listed in TCM_KBD_MAP */
    if (key == MFB_KB_KEY_SPACE)
      ev.event.key_code = 0x20;
    else if (key >= MFB_KB_KEY_A && key <= MFB_KB_KEY_Z)
      ev.event.key_code = (uint16_t)(key + 0x20); /* A-Z → a-z */
    else if (key >= MFB_KB_KEY_0 && key <= MFB_KB_KEY_9)
      ev.event.key_code = (uint16_t)(key);
  }

  (void)w;
  (void)mod; /* unused in this callback */

  /* During the post-guest hold phase (g_cpu_finished set), any key press
     dismisses the window — the event is not forwarded to the (dead) guest. */
  if (pressed && g_cpu.finished) {
    g_dismiss = true;
    return;
  }

  peripheral_kbd_enqueue(ev);
}

/* Format frequency as "X.XXHz/kHz/MHz/GHz".  freq_hz is non-negative. */
static void format_freq(double freq_hz, char *out, size_t out_len) {
  if (freq_hz >= 1e9)
    snprintf(out, out_len, "%3.2fGHz", round(freq_hz / 1e7) / 100);
  else if (freq_hz >= 1e6)
    snprintf(out, out_len, "%3.2fMHz", round(freq_hz / 1e4) / 100);
  else if (freq_hz >= 1e3)
    snprintf(out, out_len, "%3.2fkHz", round(freq_hz / 1e1) / 100);
  else
    snprintf(out, out_len, "%3.2fHz", freq_hz);
}

/* Format uptime as "HH:MM:SS.XXX" from total nanoseconds. */
static void format_uptime(uint64_t total_ns, char *out, size_t out_len) {
  uint64_t total_ms = total_ns / 1000000ULL;
  uint64_t hours = total_ms / 3600000;
  uint64_t minutes = (total_ms / 60000) % 60;
  uint64_t seconds = (total_ms / 1000) % 60;
  uint64_t miliseconds = total_ms % 1000;
  snprintf(out, out_len, "%02llu:%02llu:%02llu.%03llu",
           (unsigned long long)hours, (unsigned long long)minutes,
           (unsigned long long)seconds, (unsigned long long)miliseconds);
}

static void host_update_title(const char *image) {
  if (!g_window)
    return;

  /* platform_timer_elapsed_ns returns delta since the previous call and resets
     the internal timestamp, so we must accumulate the per-frame deltas. */
  g_cpu.acc_ns += platform_timer_elapsed_ns(
      (PlatformTimer *)&g_cpu.uptime_timer,
      g_cpu.finished ? NULL : (uint64_t *)&g_cpu.uptime_ns);

  if (g_cpu.acc_ns < 200000000ULL)
    return; /* Update title at most once per second. */

  uint64_t now_count = g_cpu.interp->get_count();
  uint64_t insns = now_count - g_cpu.last_count;
  double freq_hz = (double)insns * 1e9 / (double)g_cpu.acc_ns;
  if (freq_hz < 0.0)
    freq_hz = 0.0;

  g_cpu.last_count = now_count;
  g_cpu.acc_ns = 0;

  char freq_str[32];
  format_freq(freq_hz, freq_str, sizeof(freq_str));

  char uptime_str[32];
  format_uptime(g_cpu.uptime_ns, uptime_str, sizeof(uptime_str));

  char title[256];
  snprintf(title, sizeof(title), "%s - %s - %s: %s - %s - %s",
           HOST_WINDOW_TITLE_PREFIX, image, g_cpu.interp->name,
           g_cpu.interp->get_status(), freq_str, uptime_str);
  mfb_set_title(g_window, title);
}

/* ==================================================================== */
/*  Render routine (called by the render thread)                        */
/* ==================================================================== */

static void host_render_frame(uint32_t filter) {
  if (!g_window)
    return;

  /* Acquire consistent console snapshots through the peripheral layer.
     Each call internally locks the console mutex so we obtain a full
     width/height/vram/mode8 snapshot without touching the mutex ourselves. */
  console_info_t pixel, text;
  peripheral_console_get_info(0, &pixel);
  peripheral_console_get_info(1, &text);

  /* Pixel console (VRAM0) — copy to main display (top-left) */

  scale_fn_t pixel_scale_f, text_scale_f;
  pixel_scale_f =
      filter ? (pixel.mode == CONSOLE_MODE_PIXEL_8 ? scale_8 : scale_32)
             : (pixel.mode == CONSOLE_MODE_PIXEL_8 ? scale_8_nearest
                                                   : scale_32_nearest);
  pixel_scale_f(pixel.vram, (int)pixel.width, (int)pixel.height,
                g_minifb_screen, SCREEN_W, SCREEN_H, OUT_X0, OUT_Y0);

  // assert(text.mode == CONSOLE_MODE_PIXEL_8);
  text_scale_f = filter ? scale_8 : scale_8_nearest;
  text_scale_f(text.vram, (int)text.width, (int)text.height, g_minifb_screen,
               SCREEN_W, SCREEN_H, OUT_X1, OUT_Y1);

  mfb_update_state r =
      mfb_update_ex(g_window, g_minifb_screen, SCREEN_W, SCREEN_H);
  if (r != MFB_STATE_OK)
    host_stop_running();

  mfb_wait_sync(g_window);
}

/* ── MinifB close callback ──────────────────────────────────────── */

/* Dedicated exit code when the user closes the window manually (128 + SIGINT),
   distinct from the guest's own BREAK(0x3ff) exit code.  See emulator_exit.h.
 */

static bool mfb_close_callback(struct mfb_window *w) {
  mfb_close(w);
  g_window = NULL;
  host_stop_running();

  /* Minifb invokes the close callback on the main/render thread; the window is
   * already gone, so exit() directly. Use "STOPPED BY USER" (not "EXIT") so it
   * is not mistaken for a guest bytecode exit, and flush stdout/stderr. */
  fprintf(stderr, "\n\n!! TCMIPS EMULATOR STOPPED BY USER !!\n");
  fflush(stderr);
  fflush(stdout);
  exit(TCM_EMULATOR_EXIT_WINDOW_CLOSED);
}

/* ==================================================================== */
/*  Thread entry points                                                  */
/* ==================================================================== */

static void cpu_thread_fn(void *arg) {
  (void)arg;

  platform_timer_init((PlatformTimer *)&g_cpu.uptime_timer);
  g_cpu.started = true;
  g_cpu.interp->execute();
  g_cpu.exit_code = g_cpu.interp->get_exit_code();

  /* Report the total number of guest instructions executed.  Must be read
   * here (after execute() returns) because the interpreter only flushes its
   * local counter to get_count() at execution end; reading it any earlier
   * inside the dispatch loop would under-count by up to one CHECK_INTERVAL
   * batch.  This line is appended to stderr (not merged into the EXIT line)
   * so existing tooling that keys off "%u" in the EXIT message is unaffected. */
  fprintf(stderr, "!! TCMIPS TOTAL INSNS = %llu !!\n",
          (unsigned long long)g_cpu.interp->get_count());

  g_cpu.finished = true;
  g_cpu.acc_ns += platform_timer_elapsed_ns(
      (PlatformTimer *)&g_cpu.uptime_timer, (uint64_t *)&g_cpu.uptime_ns);
}

/* Main-thread render loop.
 *
 * On macOS the Cocoa windowing backend used by minifb requires that all
 * display/window calls (open, update, close, event handling) happen on the
 * process main thread.  Therefore this loop runs synchronously in host_run()
 * while the CPU emulator is executing on a background worker thread.
 *
 * When @p wait_for_dismiss is true the loop enters a hold phase after the
 * guest terminates: it keeps redrawing the last frame and pumping Cocoa/Win32
 * events until the user presses any key or closes the window.  This prevents
 * the window from disappearing before the user even sees it when the guest
 * exits immediately. */
static void main_render_loop(const char *image, uint32_t fps,
                             bool wait_for_dismiss, bool filter) {
  g_window = mfb_open_ex(HOST_WINDOW_TITLE_PREFIX, SCREEN_W, SCREEN_H,
                         MFB_WF_RESIZABLE);
  if (!g_window) {
    fprintf(stderr, "Failed to create display window\n");
    host_stop_running();
    return;
  }

  mfb_set_target_fps(fps);
  mfb_set_keyboard_callback(g_window, mfb_keyboard_fn);
  mfb_set_close_callback(g_window, mfb_close_callback);

  /* Show a blank frame immediately so the window appears at once */
  memset(g_minifb_screen, 0, sizeof(g_minifb_screen));
  mfb_update_ex(g_window, g_minifb_screen, SCREEN_W, SCREEN_H);

  PlatformTimer frame_timer;
  platform_timer_init(&frame_timer);
  uint64_t interval_ns = (fps > 0) ? (1000000000ULL / fps) : 0;

  while (host_is_running() && !g_cpu.finished) {
    if (g_cpu.started) {
      host_render_frame(filter);
      host_update_title(image);
    }

    if (interval_ns > 0) {
      uint64_t elapsed = platform_timer_elapsed_ns(&frame_timer, NULL);
      if (elapsed < interval_ns) {
        uint32_t remaining_ms =
            (uint32_t)((interval_ns - elapsed) / 1000000ULL);
        if (remaining_ms > 0)
          platform_sleep_ms(remaining_ms);
      }
    } else {
      /* Unlimited FPS — yield briefly so we don't starve the CPU thread. */
      platform_sleep_ms(0);
    }
  }

  /* Draw the last guest frame once. */
  host_render_frame(filter);

  /* Post-guest hold: keep the window alive, redraw the frozen frame and
     pump events so Cocoa actually puts it on screen.  Exit on key press,
     close-window, or if the user passed -hold=0. */
  if (wait_for_dismiss && g_window) {
    /* The hold loop does NOT check host_is_running() — by this point
       g_emu_running may already have been cleared by a non-OK status
       from mfb_update_ex during the final host_render_frame(), yet
       the window is still open and fully functional.  We just pump
       events and redraw until the user dismisses the window. */
    while (g_window && !g_dismiss) {
      /* mfb_update_ex pumps the native event loop (keyboard callbacks fire
         here, which can set g_dismiss).  mfb_wait_sync throttles us to the
         display refresh rate so we don't burn CPU doing nothing.
         We ignore the return code here — if the window was actually closed,
         g_window will be NULL after the close callback fires. */
      host_update_title(image);
      mfb_update_ex(g_window, g_minifb_screen, SCREEN_W, SCREEN_H);
      mfb_wait_sync(g_window);
    }
  }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

bool host_set_stdin_path(const char *path) {
  /* Close any previously-opened stdin file */
  if (g_stdin_fp) {
    fclose(g_stdin_fp);
    g_stdin_fp = NULL;
  }

  if (path && path[0] != '\0') {
    g_stdin_fp = fopen(path, "rb");
    if (g_stdin_fp) {
      peripheral_set_stdin(g_stdin_fp);
      return true;
    } else {
      return false;
    }
  }

  return false;
}

/* ── Internal ───────────────────────────────────────────────────────── */

static void host_shutdown(void) {
  host_stop_running();

  /* Tear down stdin */
  if (g_stdin_fp) {
    fclose(g_stdin_fp);
    g_stdin_fp = NULL;
  }
  peripheral_set_stdin(NULL);

  /* Tear down display */
  if (g_window) {
    mfb_close(g_window);
    g_window = NULL;
  }
}

int host_run(const char *image, uint32_t fps, bool wait_for_dismiss,
             bool filter, const char *mode, uint32_t speed_mhz) {
  /* Reset running state in case host_run is executed multiple times. */
  g_emu_running = true;
  g_dismiss = false;

  /* Reset the fast interpreter's speed clamp so a previous run's value can
   * never leak into this one (the setter below re-applies it when non-zero). */
  cpu_fast_set_speed_mhz(0);

  /* Interpreter selection: fast > jit > debug (first match wins). */
  const cpu_interpreter_t *interps[] = {
      &g_interp_fast,
      &g_interp_jit,
      &g_interp_debug,
  };
  const cpu_interpreter_t *interp = NULL;
  for (uint32_t i = 0; i < sizeof(interps) / sizeof(interps[0]); i++)
    if (strcasecmp(interps[i]->name, mode) == 0)
      interp = interps[i];

  if (!interp) {
    fprintf(stderr, "Error: unknown mode '%s'\n", mode);
    return TCM_EMULATOR_EXIT_INTERNAL;
  }

  /* Only the fast interpreter honours the speed clamp; jit/debug ignore it. */
  if (interp == &g_interp_fast && speed_mhz != 0)
    cpu_fast_set_speed_mhz(speed_mhz);

  g_cpu.interp = interp;
  platform_thread_t cpu_thread;

  /* Start the CPU emulator on a background thread. */
  if (!platform_thread_create(&cpu_thread, cpu_thread_fn, NULL)) {
    fprintf(stderr, "Error: failed to create CPU thread\n");
    host_stop_running();
    host_shutdown();
    return TCM_EMULATOR_EXIT_INTERNAL;
  }

  /* Run the minifb window / render loop on the process main thread.
   * On macOS this is required by the Cocoa backend. */
  main_render_loop(image, fps, wait_for_dismiss, filter);

  /* Render loop ended (window closed or CPU finished) — ask the CPU to stop. */
  host_stop_running();
  platform_thread_join(&cpu_thread);

  host_shutdown();
  return (int)g_cpu.exit_code;
}
