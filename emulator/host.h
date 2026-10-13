/*
 * Host Layer — Public API
 *
 * Encapsulates everything that bridges the emulator to the host OS:
 * window creation / teardown, VRAM rendering, keyboard translation,
 * and semihost stdin configuration.  Guest code / CPU core have zero
 * knowledge of this layer.
 *
 * This layer also owns the emulator "running" flag used by both the CPU
 * thread and the render thread for graceful shutdown.
 */

#ifndef HOST_H
#define HOST_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

/* TCMIPS_VERSION_STRING comes from config.h (sourced from cmake/version.cmake
   via cmake/config.cmake). Adjacent string literals are concatenated by the
   compiler, so the window title and usage banner include the version. */

/* Application window title prefix; the host render loop appends runtime
   statistics (uptime and effective frequency) to this string. */
#define HOST_WINDOW_TITLE_PREFIX "TCMIPS-Emulator v" TCMIPS_VERSION_STRING

/* ───────────────────────────────────────────────────────────────────── */

/**
 * Configure semihost stdin via a host file.
 * @param path  Path to a binary file; the host will read from it
 *              whenever the guest issues syscall(NUM_SEMIHOST, rd=0, …).
 *              Pass NULL to disable.
 */
bool host_set_stdin_path(const char *path);

/** Query whether the emulator is still running.  May be called from any
 *  thread; cheap enough to be checked in the CPU hot loop. */
bool host_is_running(void);

/** Request a graceful emulator shutdown.  The CPU and render threads will
 *  observe this at their next running-flag check and exit. */
void host_stop_running(void);

/** Execute the guest image.
 *
 *  host_run() spawns two threads:
 *    - a CPU thread that runs the interpreter loop until BREAK or shutdown,
 *    - a render thread that owns the minifb window and refreshes VRAM at
 *      @p fps.
 *  The guest mirror is expected to have been zeroed and the image loaded
 *  already (memory_init()); host_run only needs the mode flags.
 *  This function only returns after both threads have exited and the
 *  window has been torn down.
 *
 *  @param image          Guest image basename (used for the window title).
 *  @param fps            Target frame rate (0=unlimited, 30, 60 recommended).
 *  @param wait_for_dismiss  If true, after the guest terminates the window
 *                           stays alive showing the last frame until the user
 *                           presses any key or closes the window.  Pass false
 *                           for automated/CI runs (-hold=0).
 *  @param filter         If true, scale the pixel console with bilinear
 *                        interpolation; otherwise use nearest-neighbor.
 *  @param mode           Interpreter selection: "fast", "jit" (default),
 *                         "debug" (also enables GDB RSP stub on TCP :1234).
 *  @param speed_mhz      Execution-speed clamp in MHz for the fast interpreter
 *                        (0 = unlimited). Ignored by jit/debug.
 *  @return  The guest exit code (from BREAK(0x3ff), GPR[5]), or 0 if the
 *           guest never issued a BREAK instruction. */
int host_run(const char *image, uint32_t fps, bool wait_for_dismiss,
             bool filter, const char *mode, uint32_t speed_mhz);

#endif /* HOST_H */