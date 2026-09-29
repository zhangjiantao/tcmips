/* SDL compatibility layer: init/quit/version. */

#include "tcmips_sdl_internal.h"

#include <dev/syscall.h>

extern void tcmips_sdl_init_state(void);

#ifdef TCM_SDL_DEBUG
extern void tcmips_sdl_debug_alloc_count_reset(void);
#endif

const SDL_version *SDL_Linked_Version(void) {
  static SDL_version version = {SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
                                SDL_PATCHLEVEL};
  return &version;
}

int SDL_Init(Uint32 flags) {
  /* Official SDL_Init is idempotent and may be called again later to add
     subsystems; it must NOT clobber an existing surface.  Only reset global
     state (and timestamp the tick base) on the first init. */
  if (!g_tcmips_sdl.initialized) {
    tcmips_sdl_init_state();
  }
  return SDL_InitSubSystem(flags);
}

int SDL_InitSubSystem(Uint32 flags) {
  /* On TCMIPS there is no OS, audio hardware or threaded runtime.  The
     subsystems we can actually service are VIDEO and TIMER; the rest are
     tracked as "initialized" so SDL1 programs that probe SDL_WasInit() see
     a sane answer, while the underlying stubs remain inert. */
  g_tcmips_sdl.initialized_subsystems |= (flags & SDL_INIT_EVERYTHING);

  /* TIMER: no explicit hardware needs configuring; the timestamp syscall is
     always available. */
  (void)flags;
  return 0;
}

void SDL_QuitSubSystem(Uint32 flags) {
  g_tcmips_sdl.initialized_subsystems &= ~(flags & SDL_INIT_EVERYTHING);
}

Uint32 SDL_WasInit(Uint32 flags) {
  if (flags == 0)
    return g_tcmips_sdl.initialized_subsystems;
  return g_tcmips_sdl.initialized_subsystems & flags;
}

void SDL_Quit(void) {
  if (g_tcmips_sdl.screen) {
    /* The SDL1 logical framebuffer is owned by this layer. */
    if (!(g_tcmips_sdl.screen->flags & SDL_PREALLOC) &&
        g_tcmips_sdl.screen->pixels) {
      SDL_free(g_tcmips_sdl.screen->pixels);
    }
    tcmips_sdl_free_format(g_tcmips_sdl.screen->format);
    SDL_free(g_tcmips_sdl.screen);
    g_tcmips_sdl.screen = NULL;
  }
  if (g_tcmips_sdl.wm_title) {
    SDL_free(g_tcmips_sdl.wm_title);
    g_tcmips_sdl.wm_title = NULL;
  }
  /* Tear down the physical pixel console if it was set up. */
  tcmips_sdl_vram_quit();
  g_tcmips_sdl.initialized_subsystems = 0;

  /* Reset ALL remaining global state so a subsequent SDL_Quit/Init cycle
     starts from a clean slate (event queue, key state, timers, mouse,
     gamma ramp, audio spec etc.).  init_state() is not called here because
     the caller is expected to SDL_Init() before using SDL again, which
     calls init_state() and re-timestamps the tick base. */
  SDL_memset(&g_tcmips_sdl, 0, sizeof(g_tcmips_sdl));

  /* Wipe the leak counter at quit time too, not just at the next init, so the
     window between SDL_Quit and the next SDL_Init reports 0 live allocs. */
#ifdef TCM_SDL_DEBUG
  tcmips_sdl_debug_alloc_count_reset();
#endif
}