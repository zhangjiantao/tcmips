#include "tcmips_sdl2_internal.h"
#include <dev/syscall.h>
#include <string.h>

/* Forward-declared in SDL2_tcmips.h only under TCM_SDL_DEBUG; we declare it
 * here unconditionally so this translation unit does not depend on the
 * public extension header. */
#ifdef TCM_SDL_DEBUG
void tcmips_sdl2_debug_alloc_count_reset(void);
#endif

// The one SDL2-for-TCMIPS global state.
tcmips_sdl2_t g_tcmips_sdl = {0};

uint64_t tcmips_sdl2_unix_ms_now(void) {
  uint32_t sec = tcm_syscall_get_timestamp();
  uint32_t ms = tcm_syscall_get_timestamp_milli();
  return ((uint64_t)sec) * 1000ULL + (uint64_t)ms;
}

void tcmips_sdl2_init_state(void) {
  SDL_memset(&g_tcmips_sdl, 0, sizeof(g_tcmips_sdl));
  g_tcmips_sdl.evt_head = 0;
  g_tcmips_sdl.evt_tail = 0;
  g_tcmips_sdl.initialized = true;
  g_tcmips_sdl.init_unix_ms = tcmips_sdl2_unix_ms_now();

  /* Clean slate for the leak counter: consecutive init/quit cycles inside
   * the same process must NOT see leftovers from the previous cycle,
   * otherwise a real leak in cycle N would only surface in the report of
   * cycle N+1 (or never, if the next cycle's leaks balance it out). */
#ifdef TCM_SDL_DEBUG
  tcmips_sdl2_debug_alloc_count_reset();
#endif
}
