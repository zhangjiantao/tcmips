/* Internal helpers for the TCMIPS SDL compatibility layer.
 * This header is NOT part of the public SDL API.
 */

#ifndef TCMIPS_SDL_INTERNAL_H
#define TCMIPS_SDL_INTERNAL_H

/* The one TCMIPS hardware surface format (mirrors libsdl2's TCMIPS_SDL_*
 * defines): R in the lowest byte, A in the highest byte, little-endian.
 * Later SDL1 logical formats (8/16/24 bpp) are converted to this at
 * VRAM-submit time in sdl_vram.c. */
#define TCMIPS_SDL_DEPTH 32
#define TCMIPS_SDL_RMASK 0x000000FFu
#define TCMIPS_SDL_GMASK 0x0000FF00u
#define TCMIPS_SDL_BMASK 0x00FF0000u
#define TCMIPS_SDL_AMASK 0xFF000000u

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL/SDL.h"
#include "sdl_vram.h"

#define TCMIPS_SDL_EVENT_QUEUE_SIZE 64
#define TCMIPS_SDL_MAX_TIMERS 8

/* One active SDL1 timer slot. */
typedef struct {
  Uint32 id;
  Uint32 interval;
  Uint32 last_tick;
  SDL_NewTimerCallback callback; /* new-style (AddTimer) callback */
  SDL_TimerCallback legacy_cb;   /* legacy (SetTimer) callback */
  void *param;
  int active;
  int is_legacy;
} tcmips_sdl_timer_t;

/* Global state for the SDL1 compatibility layer. */
typedef struct {
  Uint32 initialized_subsystems;

  /* Set once tcmips_sdl_init_state() has run (i.e. the first SDL_Init).
   * Distinguishes the first init (which must reset state + timestamp) from
   * later idempotent SDL_Init calls that must not clobber an existing
   * surface. */
  int initialized;

  /* Mono-intl ms base at SDL_Init; SDL_GetTicks is relative to this. */
  Uint64 init_unix_ms;

  /* The single window / screen surface. */
  SDL_Surface *screen;

  /* Simple FIFO event queue for SDL1 events. */
  SDL_Event evt_queue[TCMIPS_SDL_EVENT_QUEUE_SIZE];
  volatile int evt_head;
  volatile int evt_tail;

  /* Key state cache sized for SDL1 key range (we use SDLK_LAST). */
  Uint8 key_state[SDLK_LAST];

  /* Mouse state (buttons). */
  Uint8 mouse_buttons;
  int mouse_x;
  int mouse_y;

  /* WM caption. */
  char *wm_title;

  /* Gamma ramp (stub storage). */
  Uint16 gamma_ramp[3 * 256];

  /* Timers. */
  tcmips_sdl_timer_t timers[TCMIPS_SDL_MAX_TIMERS];
  Uint32 next_timer_id;

  /* Audio spec registered by SDL_OpenAudio. */
  SDL_AudioSpec audio_desired;
  SDL_AudioSpec audio_obtained;
  int audio_opened;
  int audio_paused;
} tcmips_sdl_state_t;

extern tcmips_sdl_state_t g_tcmips_sdl;

/* Format / palette helpers shared between implementation files. */
extern SDL_PixelFormat *tcmips_sdl_alloc_format(int bpp, Uint32 rmask,
                                                Uint32 gmask, Uint32 bmask,
                                                Uint32 amask);
extern void tcmips_sdl_free_format(SDL_PixelFormat *fmt);

/* Logging helper for unimplemented functions. */
#ifndef TCMIPS_SDL_STUB_LOG_ENABLED
#define TCMIPS_SDL_STUB_LOG_ENABLED 1
#endif

#if TCMIPS_SDL_STUB_LOG_ENABLED
#include <stdio.h>
#define TCMIPS_SDL_STUB_MSG(name)                                              \
  do {                                                                         \
    static int __logged = 0;                                                   \
    if (!__logged) {                                                           \
      __logged = 1;                                                            \
      SDL_SetError("[SDL_STUB] %s not implemented", (name));                   \
      fprintf(stdout, "[SDL_STUB] %s not implemented\n", (name));              \
      fflush(stdout);                                                          \
    }                                                                          \
  } while (0)
#else
#define TCMIPS_SDL_STUB_MSG(name)                                              \
  do {                                                                         \
  } while (0)
#endif

/* Memory leak detection.
 *
 * With TCM_SDL_DEBUG the public SDL_stdinc.h declares the SDL_malloc family
 * as real extern functions instead of libc aliases.  The actual wrapper
 * implementations live in tcmips_sdl_state.c (counting live allocations and
 * exposing tcmips_sdl_debug_alloc_count()).  This header intentionally does
 * NOT redefine SDL_malloc/etc — the library internal .c files simply call
 * `SDL_malloc(...)` and the linker resolves to the strong wrapper symbol
 * (or, in non-debug builds, the libc alias macro from SDL_stdinc.h).
 *
 * The query is also re-exported to user code via SDL_tcmips.h, so tests do
 * not have to declare it manually. */

#ifdef __cplusplus
}
#endif

#endif /* TCMIPS_SDL_INTERNAL_H */
