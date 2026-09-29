#include "tcmips_sdl2_internal.h"
#include <SDL.h>

Uint32 SDL_GetTicks(void) {
  uint64_t now = tcmips_sdl2_unix_ms_now();
  return (Uint32)(now - g_tcmips_sdl.init_unix_ms);
}

Uint64 SDL_GetTicks64(void) {
  return tcmips_sdl2_unix_ms_now() - g_tcmips_sdl.init_unix_ms;
}

Uint64 SDL_GetPerformanceCounter(void) {
  return (Uint64)tcm_syscall_get_timestamp_nano();
}

Uint64 SDL_GetPerformanceFrequency(void) {
  // Nanosecond resolution: 1 tick per nanosecond -> 1e9 Hz.
  return 1000000000ULL;
}

void SDL_Delay(Uint32 ms) {
  /* TCMIPS is single-threaded with no RTOS; busy-wait is the only option.
   * Sub-frame (<100 ms) delays return immediately: the TCMIPS core is slow,
   * so a short busy-wait would burn a large fraction of a frame for almost
   * no observable timing effect — same trade-off as the SDL1 layer. */
  if (ms < 100)
    return;

  uint64_t start = SDL_GetTicks64();
  while ((SDL_GetTicks64() - start) < ms) {
    /* spin */
  }
}
