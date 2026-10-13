//// tcm_bsp.h — BSP board-support-package conventions.
//
// Constants and invariants that describe how the TCMIPS board-support layer
// (start.c / posix.c) partitions the guest's POSIX-facing resources and talks
// to the emulator. These are BSP POLICY, not hardware layout (see tcm_config.h,
// generated from cmake/config.cmake) and not any single library's concern:
// they live here so the startup glue, the POSIX syscall glue, and any code that
// reasons about the guest<->emulator boundary can reference one named single
// source of truth.

#ifndef TCM_BSP_H
#define TCM_BSP_H

#include <stdint.h>

/* ── File-descriptor partition ─────────────────────────────────────────── */
//
// File descriptor where the in-memory filesystem (libmemfs) begins allocating
// its regular descriptors. The POSIX standard reserves 0/1/2 for
// stdin/stdout/stderr, which the BSP console keeps; memfs therefore starts at
// STDERR_FILENO + 1 == 3. libmemfs itself imposes no such policy — it simply
// takes this value via memfs_init(base_fd) and allocates fds from there upward.
// posix.c routes read()/write()/lseek()/close() by comparing against this value:
// fd <  MEMFS_BASE_FD  -> stdin/stdout/stderr (console / semihost bridge)
// fd >= MEMFS_BASE_FD  -> memfs regular file
#define MEMFS_BASE_FD 3

/* ── Guest exit protocol (BREAK 0x3ff) ─────────────────────────────────── */
//
// posix.c's __shutdown() ends a guest program by executing `break 0x3ff` in a
// loop with:
//   $a0 = reason  (one of TCM_SHUTDOWN_* below)
//   $a1 = param   (for EXIT: the process exit code, already masked)
//
// The reason/disposition is the *first* register; the numeric exit code is the
// *second*. The emulator treats `break 0x3ff` as the guest-termination signal
// (see emulator/emulator_exit.h and emulator/README.md).

/* Reason values passed in $a0 by __shutdown(). */
#define TCM_SHUTDOWN_EXIT 0x00000000 /* normal program exit              */
#define TCM_SHUTDOWN_DEAD 0xdeadbeef /* fatal fault / abort              */

/* Mask applied to a guest exit code before it is handed to the emulator.
 *
 * The guest exit code occupies [0, 127]; the emulator reserves [128, 255] for
 * its OWN error conditions (130 window-closed, 131 memory-fault, ...). Masking
 * the guest status with 0x7F guarantees the two ranges never collide — a guest
 * calling exit(130) still yields 2, so "window closed by user" (130) stays
 * unambiguous (see emulator/emulator_exit.h for the emulator-side half of this
 * contract). */
#define TCM_GUEST_EXIT_MASK 0x7Fu

/* ── Startup stack pointer alignment ───────────────────────────────────── */
//
// TCM_SYMBOL_START (the naked boot stub in start.c) sets $sp with a single
// `lui $sp, <hi16>` — it only loads the HIGH 16 bits of TCM_STACK_TOP_ADDR, so
// that address MUST be 64 KB (0x10000) aligned or the low bits of SP are left
// garbage. start.c enforces this with a compile-time #error; the invariant is
// recorded here so the reason is not lost in an inline assertion.
//
// (TCM_SYMBOL_START / TCM_SYMBOL_CTORS / TCM_SYMBOL_DTORS themselves are the
// linker-facing symbol names defined in tcm_format.h, not here.)

/* ── Standard I/O backend selection ────────────────────────────────────── */
//
// Which backend serves stdin/stdout/stderr is chosen by the picolibc build-time
// switch POSIX_CONSOLE (picolibc.h.in `#cmakedefine POSIX_CONSOLE`), not by any
// constant here:
//   * POSIX_CONSOLE defined   -> the POSIX file layer below takes over; fds
//                                0/1/2 are served via read(0)/write(1,2), which
//                                post to the semihost bridge when the emulator
//                                wired a -stdin and falls back to the text
//                                console otherwise.
//   * POSIX_CONSOLE undefined -> stdin/stdout/stderr are wired straight to the
//                                in-game text console (console_getc/putc).
// posix.c branches on this macro at compile time. It is documented here only
// for context; do NOT #define it in this header.

#endif // TCM_BSP_H
