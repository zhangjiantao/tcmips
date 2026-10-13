/*
 * Peripherals & Syscall Dispatch — Public Interface
 *
 * All guest-code peripheral interactions funnel through the MIPS
 * SYSCALL instruction, which the CPU core dispatches to this module.
 *
 *      ┌─────────────────────────────────────────────┐
 *      │  Guest TCMIPS code                           │
 *      │  e.g. "syscall 1"  (get timestamp)           │
 *      └──────────────┬──────────────────────────────┘
 *                     │  CPU → syscall(nr, rd, a0, a1)
 *                     ▼
 *      ┌─────────────────────────────────────────────┐
 *      │  Syscall dispatch (this module)                  │
 *      │  - timestamp    → platform_mono_now()            │
 *      │  - console      → VRAM state setters             │
 *      │  - keyboard     → 32-slot ring buffer            │
 *      │  - palette      → init / upload / set_offset     │
 *      │  - semihost     → stdin/stdout/stderr
 *      └─────────────────────────────────────────────┘
 */

#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include <stdint.h>
#include <stdio.h>

#include "dev/console.h"
#include "dev/keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Syscall dispatch (called by CPU core) ────────────────────── */

/**
 * Inject a guest SYSCALL into the host.
 * @param nr   Syscall number      (5-bit value, e.g.
 * __NR_tcm_syscall_get_timestamp)
 * @param rd   Sub-function       (e.g. 0=sec, 1=msec, … for timestamps)
 * @param a0   Argument 0         (buffer addr for semihost, command for
 * console)
 * @param a1   Argument 1         (length for semihost, param for console)
 * @return     Return value placed in $v0 (or 0 for void syscalls)
 */
uint32_t syscall(uint32_t nr, uint32_t rd, uint32_t a0, uint32_t a1);

/* ── Keyboard event ring buffer ─────────────────────────────── */

/** Enqueue a host keyboard event; discards the oldest when full. */
void peripheral_kbd_enqueue(TcmKeyboardEvent ev);

/* ── Console info snapshot ───────────────────────────────────── */
/** Immutable snapshot of one virtual console (pixel or text).
 *  Acquired via peripheral_console_get_info() which internally locks the
 *  console state so the caller gets a consistent snapshot without needing to
 *  know about the mutex.
 *
 *  Fields:
 *    vram   — pointer into g_emu_memory (vram_offset already applied)
 *    width  — pixel count (pixel console) or character columns (text console)
 *    height — pixel rows (pixel console) or character rows (text console)
 *    mode   — 0 = 32-bit RGBA, 1 = 8-bit RGB332 palette (pixel console only;
 *             text console reports 0)
 */
typedef struct {
  uint8_t *vram;
  uint32_t width;
  uint32_t height;
  tcm_console_mode_t mode;
} console_info_t;

/**
 * Copy the current VRAM metadata (width, height, vram base, mode) for the
 * console identified by @p rd (0 = pixel, 1 = text) into @p info.
 * Thread-safe — the snapshot is taken under the console mutex inside this
 * function.
 */
void peripheral_console_get_info(uint32_t rd, console_info_t *info);

/* ── Peripheral lifecycle ─────────────────────────────────────── */

/** Initialise all peripheral sub-modules (console, keyboard) and their
 *  internal mutexes.  Must be called once before any thread uses them. */
void peripheral_init(void);

/** Destroy all peripheral sub-module mutexes.  Safe only after both the CPU
 *  and render threads have exited. */
void peripheral_destroy(void);

/* ── Semihost stdin (set by host layer) ───────────────────────── */

/**
 * Associate a host FILE* with the guest's semihost stdin (rd==0).
 * Pass NULL to disable.
 */
void peripheral_set_stdin(FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* PERIPHERAL_H */
