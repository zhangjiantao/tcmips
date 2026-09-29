#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <dev/syscall.h>
#include <stdint.h>
#include <string.h>

#ifdef TCM_SDL_DEBUG
void tcmips_sdl2_debug_alloc_count_reset(void);
#endif

static Uint32 s_initialized = 0;

int SDL_Init(Uint32 flags) {
  /* Official SDL_Init is idempotent and may be called again later to add
     subsystems; it must NOT clobber an already-created window/renderer/
     event queue.  Only reset global state (and re-timestamp the tick base)
     on the first init, detected via g_tcmips_sdl.initialized. */
  if (!g_tcmips_sdl.initialized) {
    tcmips_sdl2_init_state();
  }
  return SDL_InitSubSystem(flags);
}

int SDL_InitSubSystem(Uint32 flags) {
  /* Subsystem init only ORs the flag; it must NOT re-init global state
     (that would clobber an already-created window/renderer/event queue). */
  s_initialized |= flags;
  return 0;
}

void SDL_QuitSubSystem(Uint32 flags) { s_initialized &= ~flags; }

void SDL_Quit(void) {
  SDL_DestroyWindow(g_tcmips_sdl.win.window);
  SDL_DestroyRenderer(g_tcmips_sdl.render.renderer);

  /* Release the active cursor if it was ever allocated (currently all cursor
     creation paths are stubs returning NULL, so this is defensive).  It is
     freed directly rather than via SDL_FreeCursor, which deliberately refuses
     to free the currently-active cursor. */
  if (g_tcmips_sdl.active_cursor) {
    SDL_free(g_tcmips_sdl.active_cursor);
  }

  /* Reset ALL remaining global state so a subsequent SDL_Quit/Init cycle
     starts from a clean slate — mirrors the SDL1 layer's SDL_Quit. */
  SDL_memset(&g_tcmips_sdl, 0, sizeof(g_tcmips_sdl));
  s_initialized = 0;

  /* Also wipe the leak counter at quit time (not just at the next init), so
     the window between SDL_Quit and the next SDL_Init reports 0 live allocs
     rather than leftover counts from the previous cycle. */
#ifdef TCM_SDL_DEBUG
  tcmips_sdl2_debug_alloc_count_reset();
#endif
}

Uint32 SDL_WasInit(Uint32 flags) {
  if (flags == 0)
    return s_initialized;
  return s_initialized & flags;
}

void SDL_SetMainReady(void) {
  // Nothing to do on bare-metal TCMIPS.
}
