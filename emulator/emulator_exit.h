/*
 * TCMIPS Emulator — Emulator Exit Codes
 *
 * Central registry for the emulator's OWN process exit codes, kept distinct
 * from the guest program's exit code.
 *
 * Convention (see AUTOTEST.md and emulator/README.md):
 *   - Guest exit code: [0, 127]   — enforced on the guest side by `_exit()`
 *     masking the status with 0x7F (tcmips/stub/stub.c).  A guest therefore can
 *     never produce a value >= 128, no matter what it passes to exit().
 *   - Emulator exit code: [128, 255] — everything in this header.  These are
 *     reserved for conditions the emulator itself detects (window closed,
 *     illegal memory access, usage/boot errors, internal faults).
 *
 * The two ranges never overlap, and both fit inside the POSIX `exit(status)`
 * 8-bit exit status (status & 0xFF), so the split is portable across Windows
 * and POSIX hosts.
 */

#ifndef EMULATOR_EXIT_H
#define EMULATOR_EXIT_H

/* User manually closed the emulator window (128 + SIGINT). */
#define TCM_EMULATOR_EXIT_WINDOW_CLOSED  130

/* Guest performed an illegal memory access (out-of-range, or read/write into
 * the 4 KB guard page). */
#define TCM_EMULATOR_EXIT_MEMORY_FAULT   131

/* Emulator invoked without an image argument (usage error). */
#define TCM_EMULATOR_EXIT_USAGE          132

/* Failed to configure the semihost stdin redirect (-stdin=<file>). */
#define TCM_EMULATOR_EXIT_BAD_STDIN      133

/* Failed to load / read the .tcm image file. */
#define TCM_EMULATOR_EXIT_LOAD_FAILED    134

/* Internal emulator fault (unknown -mode, CPU thread creation failure, JIT
 * translation failure, etc.). */
#define TCM_EMULATOR_EXIT_INTERNAL       135

#endif /* EMULATOR_EXIT_H */
