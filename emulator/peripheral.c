/*
 * Peripherals & Syscall Dispatch — Implementation
 *
 * Implements the host-side behaviour of the TCMIPS hardware peripherals:
 *   1. timestamp   — forwarded to platform_mono_now()
 *   2. console     — manages VRAM mode / resolution / offset state
 *   3. keyboard    — 32-slot ring buffer fed by host keyboard events
 *   4. palette     — palette init / upload / set_offset for pixel console
 *   5. semihost    — bridges stdin/stdout/stderr between guest and host
 *      (stdin is backed by a host-managed FILE*, set via -stdin=arg)
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "peripheral.h"
#include "platform.h"

#include "dev/syscall.h"

/* ==================================================================== */
/*  Console VRAM state (pixel + text)                                   */
/* ==================================================================== */

/** Internal console descriptor.  Written by the guest via syscall_console,
 *  read through peripheral_console_get_info(). Not visible outside this TU. */
static console_info_t pixel_console;
static console_info_t text_console;

/* Mutex protecting the two TcmConsoleVram structs (console syscalls vs. host
   draw thread).  Initialised/destroyed by peripheral_init/peripheral_destroy.
 */
static platform_mutex_t g_console_mutex;

void peripheral_console_get_info(uint32_t rd, console_info_t *info) {
  platform_mutex_lock(&g_console_mutex);
  *info = (rd == 0) ? pixel_console : text_console;
  platform_mutex_unlock(&g_console_mutex);
}

/* ==================================================================== */
/*  Keyboard ring buffer                                             */
/* ==================================================================== */

#define KBD_QUEUE_CAP 32 /* capacity (slots) of the keyboard ring buffer */

/* Fixed-capacity FIFO ring buffer of keyboard events.
 *
 * Each physical key press is delivered by the host keyboard callback as
 * two events (press + release), and SDL-based guests drain the whole queue
 * at once every SDL_PumpEvents().  A generous capacity avoids dropping
 * fast typing while holding down keys.  When the buffer is full the oldest
 * unread event is discarded (most-recent-wins) and g_kbd_overflowed is set.
 */
static TcmKeyboardEvent g_kbd_queue[KBD_QUEUE_CAP];
static uint32_t g_kbd_head = 0; /* index of oldest pending event      */
static uint32_t g_kbd_tail = 0; /* index of next free slot (== head ⇒ empty) */
static uint32_t g_kbd_count = 0;
static uint32_t g_kbd_overflowed = 0; /* events dropped when full        */

/* Mutex protecting g_kbd_queue/g_kbd_head/g_kbd_tail/g_kbd_count.
   Initialised/destroyed by peripheral_init/peripheral_destroy. */
static platform_mutex_t g_kbd_mutex;

/* ==================================================================== */
/*  Peripheral lifecycle                                                */
/* ==================================================================== */

void peripheral_init(void) {
  platform_mutex_init(&g_console_mutex);
  platform_mutex_init(&g_kbd_mutex);
}

void peripheral_destroy(void) {
  platform_mutex_destroy(&g_console_mutex);
  platform_mutex_destroy(&g_kbd_mutex);
}

/* Enqueue one event.  When full, discards the oldest (most-recent-wins)
 * and counts the drop in g_kbd_overflowed.  Caller must hold g_kbd_mutex. */
static void kbd_push(TcmKeyboardEvent ev) {
  if (g_kbd_count == KBD_QUEUE_CAP) {
    g_kbd_head = (g_kbd_head + 1) % KBD_QUEUE_CAP;
    g_kbd_overflowed++;
  } else {
    g_kbd_count++;
  }
  g_kbd_queue[g_kbd_tail] = ev;
  g_kbd_tail = (g_kbd_tail + 1) % KBD_QUEUE_CAP;
}

/* Dequeue one event into *out.  Returns true if an event was available,
 * false when the queue is empty (leaf *out untouched).  Caller must hold
 * g_kbd_mutex. */
static bool kbd_pop(TcmKeyboardEvent *out) {
  if (g_kbd_count == 0)
    return false;
  *out = g_kbd_queue[g_kbd_head];
  g_kbd_head = (g_kbd_head + 1) % KBD_QUEUE_CAP;
  g_kbd_count--;
  return true;
}

void peripheral_kbd_enqueue(TcmKeyboardEvent ev) {
  platform_mutex_lock(&g_kbd_mutex);
  kbd_push(ev);
  platform_mutex_unlock(&g_kbd_mutex);
}

/* ==================================================================== */
/*  Syscall handlers                                                    */
/* ==================================================================== */

/*  SYSCALL 1 — get_timestamp                                         */
static uint32_t syscall_timestamp(uint32_t rd, uint32_t a0, uint32_t a1) {
  (void)a0;
  (void)a1;
  struct timespec ts;
  platform_utc_time(&ts);
  switch (rd) {
  default:
    return 0;
  case 0:
    return ts.tv_sec; /* seconds    */
  case 1:
    return (ts.tv_nsec / 1000000) % 1000; /* milliseconds */
  case 2:
    return (ts.tv_nsec / 1000) % 1000000; /* microseconds */
  case 3:
    return ts.tv_nsec; /* nanoseconds  */
  }
}

/*  SYSCALL 3 — console                                               */
static uint32_t syscall_console(uint32_t rd, uint32_t a0, uint32_t a1) {
  platform_mutex_lock(&g_console_mutex);

  console_info_t *c = rd == 0 ? &pixel_console : &text_console;
  switch ((tcm_console_cmd_t)a0) {
  case CONSOLE_CMD_SET_MODE:
    c->mode = a1;
    break;
  case CONSOLE_CMD_SET_RESOLUTION:
    a1 += 1;
    c->width = a1 * 4;
    c->height = a1 * 3;
    break;
  case CONSOLE_CMD_SET_DATA_OFFSET:
    c->vram = g_emu_memory + a1;
    break;
  default:
    break;
  }

  platform_mutex_unlock(&g_console_mutex);
  return 0;
}

/*  SYSCALL 4 — read_keyboard                                         */
static uint32_t syscall_read_keyboard(uint32_t rd, uint32_t a0, uint32_t a1) {
  (void)rd;
  (void)a0;
  (void)a1;
  platform_mutex_lock(&g_kbd_mutex);
  TcmKeyboardEvent ev;
  if (!kbd_pop(&ev))
    ev.raw_value = 0; /* empty queue → not_empty == 0 */
  platform_mutex_unlock(&g_kbd_mutex);
  return ev.raw_value;
}

/*  SYSCALL 5 — palette                                               */
static uint32_t syscall_palette(uint32_t rd, uint32_t a0, uint32_t a1) {
  static uint8_t palette[256] = {0};
  static uint32_t pal_offset = 0;

  switch (rd) {
  default:
    return 0;

  // tcm_syscall_palette_init
  case 0:
    if (a0 + 4 <= 256)
      *(uint32_t *)(palette + a0) = a1;
    return 0;

  // tcm_syscall_palette_upload
  case 1:
    g_emu_memory[pal_offset + 0] = palette[a0 & 0xff];
    g_emu_memory[pal_offset + 1] = palette[(a0 >> 8) & 0xff];
    g_emu_memory[pal_offset + 2] = palette[(a0 >> 16) & 0xff];
    g_emu_memory[pal_offset + 3] = palette[(a0 >> 24) & 0xff];
    g_emu_memory[pal_offset + 4] = palette[a1 & 0xff];
    g_emu_memory[pal_offset + 5] = palette[(a1 >> 8) & 0xff];
    g_emu_memory[pal_offset + 6] = palette[(a1 >> 16) & 0xff];
    g_emu_memory[pal_offset + 7] = palette[(a1 >> 24) & 0xff];
    return 0;

  case 2:
    pal_offset = a0;
    return 0;
  }
}

/*  SYSCALL 7 — semihost (stdin/stdout/stderr bridge)                  */

/** Backing FILE* for guest semihost stdin (rd==0). Set by host layer. */
static FILE *g_stdin_fp = NULL;

void peripheral_set_stdin(FILE *fp) { g_stdin_fp = fp; }

static FILE *peripheral_get_stdin(void) { return g_stdin_fp; }

static uint32_t syscall_semihost(uint32_t rd, uint32_t a0, uint32_t a1) {
  switch (rd) {
  case 0: { /* ── stdin ─────────────────────────────────────────── */
    FILE *fp = peripheral_get_stdin();
    if (!fp)
      return 0xFFFFFFFFu; /* no file → "not wired up", NOT EOF */
    uint32_t addr = a0;
    uint32_t len = a1;
    if (addr >= TCM_MEMORY_SIZE)
      return 0;
    if (addr + len > TCM_MEMORY_SIZE)
      len = TCM_MEMORY_SIZE - addr;
    if (len == 0)
      return 0;
    uint32_t n = (uint32_t)fread(g_emu_memory + addr, 1, len, fp);
    return n;
  }
  case 1: { /* ── stdout ────────────────────────────────────────── */
    uint32_t addr = a0;
    uint32_t len = a1;
    if (addr >= TCM_MEMORY_SIZE)
      return 0;
    if (addr + len > TCM_MEMORY_SIZE)
      len = TCM_MEMORY_SIZE - addr;
    return (uint32_t)fwrite(g_emu_memory + addr, 1, len, stdout);
  }
  case 2: { /* ── stderr ────────────────────────────────────────── */
    uint32_t addr = a0;
    uint32_t len = a1;
    if (addr >= TCM_MEMORY_SIZE)
      return 0;
    if (addr + len > TCM_MEMORY_SIZE)
      len = TCM_MEMORY_SIZE - addr;
    return (uint32_t)fwrite(g_emu_memory + addr, 1, len, stderr);
  }
  default:
    return 0;
  }
}

/* ==================================================================== */
/*  Syscall dispatcher                                                  */
/* ==================================================================== */

uint32_t syscall(uint32_t nr, uint32_t rd, uint32_t a0, uint32_t a1) {
  switch (nr) {
  default:
    return 0;

  case __NR_tcm_syscall_get_timestamp:
    return syscall_timestamp(rd, a0, a1);

  case __NR_tcm_syscall_console:
    return syscall_console(rd, a0, a1);

  case __NR_tcm_syscall_read_keyboard:
    return syscall_read_keyboard(rd, a0, a1);

  case __NR_tcm_syscall_palette:
    return syscall_palette(rd, a0, a1);

  case __NR_tcm_syscall_semihost:
    return syscall_semihost(rd, a0, a1);
  }
}