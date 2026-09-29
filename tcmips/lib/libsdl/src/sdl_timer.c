/* SDL compatibility layer: timers.
 *
 * Uses the TCMIPS native timestamp syscalls directly (no SDL2 dependency).
 * SDL_GetTicks reports millisecond ellapsed since SDL_Init; SDL_Delay is a
 * single-threaded busy-wait on the same clock.
 *
 * Upstream SDL-1.2 baseline: release-1.2.15,
 * commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
 */

#include "tcmips_sdl_internal.h"

#include <dev/syscall.h>

static uint64_t tcmips_sdl_unix_ms_now(void) {
  uint32_t sec = tcm_syscall_get_timestamp();
  uint32_t ms = tcm_syscall_get_timestamp_milli();
  return ((uint64_t)sec) * 1000ULL + (uint64_t)ms;
}

Uint32 SDL_GetTicks(void) {
  uint64_t now = tcmips_sdl_unix_ms_now();
  return (Uint32)(now - g_tcmips_sdl.init_unix_ms);
}

void SDL_Delay(Uint32 ms) {
  /* TCMIPS is single-threaded with no RTOS; busy-wait is the only option. */
  /* Sub-frame (<100 ms) delays return immediately instead of spinning.  The
   * TCMIPS core runs very slowly, so a short busy-wait would burn a large
   * fraction of a frame for almost no observable timing effect; games are
   * expected to schedule longer pauses via SDL_Delay or SDL_SetTimer. */
  if (ms < 100)
    return;

  uint32_t start = SDL_GetTicks();
  while ((SDL_GetTicks() - start) < ms) {
    /* spin */
  }
}

int SDL_SetTimer(Uint32 interval, SDL_TimerCallback callback) {
  /* SDL1 legacy timer: only ONE such timer may be active at a time. */
  for (int i = 0; i < TCMIPS_SDL_MAX_TIMERS; ++i) {
    if (g_tcmips_sdl.timers[i].active && g_tcmips_sdl.timers[i].is_legacy) {
      g_tcmips_sdl.timers[i].active = 0;
    }
  }
  if (!callback || interval == 0) {
    return 0; /* cleared */
  }
  for (int i = 0; i < TCMIPS_SDL_MAX_TIMERS; ++i) {
    if (!g_tcmips_sdl.timers[i].active) {
      g_tcmips_sdl.timers[i].id = g_tcmips_sdl.next_timer_id++;
      g_tcmips_sdl.timers[i].interval = interval;
      g_tcmips_sdl.timers[i].last_tick = SDL_GetTicks();
      g_tcmips_sdl.timers[i].callback = NULL;
      g_tcmips_sdl.timers[i].legacy_cb = callback;
      g_tcmips_sdl.timers[i].param = NULL;
      g_tcmips_sdl.timers[i].is_legacy = 1;
      g_tcmips_sdl.timers[i].active = 1;
      return 0;
    }
  }
  SDL_SetError("No free SDL timer slot");
  return -1;
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_NewTimerCallback callback,
                         void *param) {
  if (!callback || interval == 0)
    return NULL;
  for (int i = 0; i < TCMIPS_SDL_MAX_TIMERS; ++i) {
    if (!g_tcmips_sdl.timers[i].active) {
      SDL_TimerID id = (SDL_TimerID)(size_t)g_tcmips_sdl.next_timer_id++;
      g_tcmips_sdl.timers[i].id = (Uint32)(size_t)id;
      g_tcmips_sdl.timers[i].interval = interval;
      g_tcmips_sdl.timers[i].last_tick = SDL_GetTicks();
      g_tcmips_sdl.timers[i].callback = callback;
      g_tcmips_sdl.timers[i].legacy_cb = NULL;
      g_tcmips_sdl.timers[i].param = param;
      g_tcmips_sdl.timers[i].is_legacy = 0;
      g_tcmips_sdl.timers[i].active = 1;
      return id;
    }
  }
  SDL_SetError("No free SDL timer slot");
  return NULL;
}

SDL_bool SDL_RemoveTimer(SDL_TimerID id) {
  if (!id)
    return SDL_FALSE;
  for (int i = 0; i < TCMIPS_SDL_MAX_TIMERS; ++i) {
    if (g_tcmips_sdl.timers[i].active &&
        (SDL_TimerID)(size_t)g_tcmips_sdl.timers[i].id == id) {
      g_tcmips_sdl.timers[i].active = 0;
      return SDL_TRUE;
    }
  }
  return SDL_FALSE;
}

/* Called from the event pump to fire due timers. */
void tcmips_sdl_update_timers(void) {
  Uint32 now = SDL_GetTicks();
  for (int i = 0; i < TCMIPS_SDL_MAX_TIMERS; ++i) {
    if (!g_tcmips_sdl.timers[i].active)
      continue;
    Uint32 elapsed = now - g_tcmips_sdl.timers[i].last_tick;
    if (elapsed < g_tcmips_sdl.timers[i].interval)
      continue;

    if (g_tcmips_sdl.timers[i].is_legacy) {
      /* Legacy SDL_SetTimer callback: Uint32 (*)(Uint32 interval),
         returning the NEXT interval (0 = stop). */
      Uint32 next =
          g_tcmips_sdl.timers[i].legacy_cb(g_tcmips_sdl.timers[i].interval);
      if (next == 0) {
        g_tcmips_sdl.timers[i].active = 0;
        continue;
      }
      g_tcmips_sdl.timers[i].interval = next;
    } else {
      /* SDL_AddTimer callback: Uint32 (*)(Uint32 interval, void* param). */
      Uint32 next = g_tcmips_sdl.timers[i].callback(
          g_tcmips_sdl.timers[i].interval, g_tcmips_sdl.timers[i].param);
      if (next != g_tcmips_sdl.timers[i].interval) {
        g_tcmips_sdl.timers[i].interval = next;
        if (next == 0) {
          g_tcmips_sdl.timers[i].active = 0;
          continue;
        }
      }
    }
    g_tcmips_sdl.timers[i].last_tick = now;
  }
}