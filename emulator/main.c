/*
 * TCMIPS Emulator — Entry Point
 *
 * Usage:
 *   TCMIPSEmulator <image.tcm> [options]
 *   TCMIPSEmulator <image.tcm> -stdin=<file> -fps=60 -filter=1 -hold=1
 *
 * Supported options:
 *   -stdin=<file>     Redirect semihost stdin from a file
 *   -fps=<1-120>      Target frame rate (default: 60; 0 = unlimited; values
 *                     outside 1-120 fall back to 0 = unlimited)
 *   -hold=<0,1>       0 = auto-dismiss on exit, 1 = hold (default)
 *   -mode=<mode>      Interpreter: jit (default), fast, debug (debug also
 *                     enables the GDB RSP stub on TCP :1234)
 *   -mhz=<MHz>        Clamp the fast interpreter to roughly this many MHz
 *                     (0 = unlimited, default).  Lets a host run approximate
 *                     real-hardware (e.g. 7 MHz) timing so slow operations
 *                     that would stutter on hardware become observable.  Only
 *                     the fast interpreter honours this; jit/debug ignore it.
 *   -filter=<0,1>     0 = nearest-neighbor (default), 1 = bilinear upscaling
 *
 * The guest's syscall(rd=0, …) reads from the -stdin file into emu_memory
 * at the address given by a0, up to a1 bytes.  syscall(rd=1, …) / (rd=2, …)
 * write to stdout / stderr respectively.
 *
 * Exit code convention:
 *   - 0, or the guest's own code (from BREAK(0x3ff), GPR[5]/$a1): normal exit.
 *   - 130 (128 + SIGINT): the user closed the window manually. The emulator
 *     prints "!! TCMIPS EMULATOR STOPPED BY USER !!" to stderr in this case so
 *     automation can distinguish "window closed" from a guest bytecode exit.
 */

#include "emulator_exit.h"
#include "host.h"
#include "memory.h"
#include "peripheral.h"
#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t parse_unsigned(const char *s) {
  char *end;
  unsigned long v = strtoul(s, &end, 10);
  return (*end == '\0') ? (uint32_t)v : 0;
}

static const char *path_basename(const char *path) {
  if (path == NULL)
    return NULL;

  const char *last_sep = NULL;
  const char *p = path;

  while (*p != '\0') {
    if (*p == '\\' || *p == '/')
      last_sep = p;
    p++;
  }

  return last_sep == NULL ? path : last_sep + 1;
}

/* ── Load a .tcm binary into memory ──────────────────────────────── */

static uint8_t *load_tcm(const char *path, uint32_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size <= 0 || size > TCM_MEMORY_SIZE) {
    fclose(f);
    return NULL;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)size);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
    free(buf);
    fclose(f);
    return NULL;
  }

  fclose(f);
  *out_size = (uint32_t)size;
  return buf;
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
  const char *stdin_path = NULL;
  const char *image_path = NULL;
  uint32_t fps = 60;   /* default 60 fps */
  uint32_t filter = 0; /* default: Nearest-Neighbor */
  uint32_t hold = 1;   /* default: hold window open after guest exits */
  bool wait_key = true;
  const char *mode = "jit"; /* default: JIT mode */
  uint32_t speed_mhz = 0;   /* default: no speed clamp (unlimited) */
  const char *prog_name = argv[0] ? argv[0] : "TCMIPSEmulator";

  /* Parse command-line arguments */
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-stdin=", 7) == 0)
      stdin_path = argv[i] + 7;
    else if (strncmp(argv[i], "-fps=", 5) == 0) {
      uint32_t val = parse_unsigned(argv[i] + 5);
      if (val > 0 && val <= 120)
        fps = val;
      else
        fps = 0;
    } else if (strncmp(argv[i], "-hold=", 6) == 0) {
      hold = parse_unsigned(argv[i] + 6);
    } else if (strncmp(argv[i], "-mode=", 6) == 0) {
      mode = argv[i] + 6;
    } else if (strncmp(argv[i], "-mhz=", 5) == 0) {
      uint32_t val = parse_unsigned(argv[i] + 5);
      if (val > 0)
        speed_mhz = val;
    } else if (strncmp(argv[i], "-filter=", 8) == 0) {
      uint32_t val = parse_unsigned(argv[i] + 8);
      if (val == 0 || val == 1)
        filter = val;
    } else if (!image_path) {
      image_path = argv[i];
    }
    /* Unknown args are silently ignored */
  }

  wait_key = (hold != 0) && (strcmp(mode, "debug") != 0);

  if (!image_path) {
    printf("%s\n\n", HOST_WINDOW_TITLE_PREFIX);
    printf("Usage: %s <image.tcm> [options]\n", prog_name);
    printf("Options:\n");
    printf("  -stdin=<file>   Redirect semihost stdin from a file\n");
    printf("  -fps=<1-120>    Target frame rate (default: 60; 0 = unlimited;\n"
           "                   values outside 1-120 fall back to unlimited)\n");
    printf("  -mode=<mode>    Interpreter: jit (default), fast, debug\n");
    printf("  -mhz=<MHz>      Clamp fast interpreter speed (0 = unlimited, default)\n");
    printf("  -hold=<0,1>     1 = keep window open after exit (default), 0 = "
           "auto-dismiss\n");
    printf("  -filter=<0,1>   0, default Nearest-Neighbor. 1, Bilinear\n");
    platform_wait_key(wait_key);
    return TCM_EMULATOR_EXIT_USAGE;
  }

  /* Configure semihost stdin (if requested) */
  if (stdin_path) {
    if (!host_set_stdin_path(stdin_path)) {
      fprintf(stderr, "Error: failed to set stdin to %s\n", stdin_path);
      platform_wait_key(wait_key);
      return TCM_EMULATOR_EXIT_BAD_STDIN;
    }
  }

  /* Load the guest image and prepare memory. */
  uint32_t imagesize = 0;
  uint8_t *image = load_tcm(image_path, &imagesize);
  if (!image) {
    fprintf(stderr, "Error: cannot load %s\n", image_path);
    platform_wait_key(wait_key);
    return TCM_EMULATOR_EXIT_LOAD_FAILED;
  }
  memory_init(image, imagesize);

  /* Initialise peripheral shared mutexes before any thread can touch them. */
  peripheral_init();

  /* Run the emulator (CPU loop; blocks until BREAK or crash).
   Returns the guest's exit code (from BREAK(0x3ff), GPR[5]).
   When wait_key is true the render loop holds the window open after
   guest termination until the user dismisses it — no need for a
   separate terminal-side platform_wait_key here. */
  int exit_code =
      host_run(path_basename(image_path), fps, wait_key, filter, mode, speed_mhz);

  /* Destroy peripheral mutexes (safe only after both threads have exited). */
  peripheral_destroy();

  /* Cleanup */
  free(image);

  return exit_code;
}