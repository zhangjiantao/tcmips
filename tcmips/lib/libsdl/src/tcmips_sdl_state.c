/* Global state for the TCMIPS SDL compatibility layer. */

#include "tcmips_sdl_internal.h"

#include <dev/syscall.h>
#include <string.h>

tcmips_sdl_state_t g_tcmips_sdl = {0};

/* Memory leak detection wrappers.
 *
 * With TCM_SDL_DEBUG defined (see SDL_stdinc.h / tcm_config.h), the public
 * headers declare the SDL_malloc / SDL_calloc / SDL_realloc / SDL_free /
 * SDL_aligned_alloc / SDL_aligned_free symbols as extern functions instead
 * of libc aliases.  The strong-symbol definitions below provide the
 * actual implementations:
 *   - they *count* live allocations into `s_alddoc_count`,
 *   - they delegate to the real libc allocator (NOT to SDL_malloc — that
 *     would recurse onto this very symbol),
 *   - `tcmips_sdl_debug_alloc_count()` lets tests read the live count,
 *   - `tcmips_sdl_debug_alloc_count_reset()` lets the SDL_Init path wipe
 *     the counter (so consecutive init/quit cycles in one process do not
 *     accumulate against each other — see tcmips_sdl_init_state below).
 *
 * In non-debug builds SDL_stdinc.h falls back to `#define SDL_malloc malloc`
 * etc., so none of these symbols are referenced and the linker drops them. */
#ifdef TCM_SDL_DEBUG
static int s_sdl_alloc_count = 0;

void *SDL_malloc(size_t size) {
  void *p = malloc(size);
  if (p)
    s_sdl_alloc_count++;
  return p;
}

void *SDL_calloc(size_t nmemb, size_t size) {
  void *p = calloc(nmemb, size);
  if (p)
    s_sdl_alloc_count++;
  return p;
}

void *SDL_realloc(void *mem, size_t size) {
  if (!mem && size > 0)
    s_sdl_alloc_count++;
  else if (mem && size == 0)
    s_sdl_alloc_count--;
  return realloc(mem, size);
}

void SDL_free(void *mem) {
  if (mem)
    s_sdl_alloc_count--;
  free(mem);
}

void *SDL_aligned_alloc(size_t alignment, size_t size) {
  void *p = aligned_alloc(alignment, size);
  if (p)
    s_sdl_alloc_count++;
  return p;
}

void SDL_aligned_free(void *mem) {
  if (mem)
    s_sdl_alloc_count--;
  free(mem);
}

size_t tcmips_sdl_debug_alloc_count(void) {
  /* Report the raw counter without clamping.  If a double-free / over-free
     drives the counter negative, wrapping it to a large unsigned value makes
     the error visible to any `live == 0` leak assertion instead of silently
     masking it to zero. */
  return (size_t)s_sdl_alloc_count;
}

void tcmips_sdl_debug_alloc_count_reset(void) { s_sdl_alloc_count = 0; }
#endif /* TCM_SDL_DEBUG */

static uint64_t tcmips_sdl_unix_ms_now(void) {
  uint32_t sec = tcm_syscall_get_timestamp();
  uint32_t ms = tcm_syscall_get_timestamp_milli();
  return ((uint64_t)sec) * 1000ULL + (uint64_t)ms;
}

void tcmips_sdl_init_state(void) {
  memset(&g_tcmips_sdl, 0, sizeof(g_tcmips_sdl));
  g_tcmips_sdl.initialized = 1;
  g_tcmips_sdl.evt_head = 0;
  g_tcmips_sdl.evt_tail = 0;
  g_tcmips_sdl.mouse_x = 0;
  g_tcmips_sdl.mouse_y = 0;
  g_tcmips_sdl.mouse_buttons = 0;
  g_tcmips_sdl.next_timer_id = 1;
  g_tcmips_sdl.init_unix_ms = tcmips_sdl_unix_ms_now();

  /* Clean slate for the leak counter: consecutive init/quit cycles inside
   * the same process must NOT see leftovers from the previous cycle,
   * otherwise a real leak in cycle N would only surface in the report of
   * cycle N+1 (or never, if the next cycle's leaks balance it out). */
#ifdef TCM_SDL_DEBUG
  tcmips_sdl_debug_alloc_count_reset();
#endif
  /* Default gamma ramp identity. */
  for (int i = 0; i < 256; ++i) {
    g_tcmips_sdl.gamma_ramp[0 * 256 + i] = (Uint16)(i << 8);
    g_tcmips_sdl.gamma_ramp[1 * 256 + i] = (Uint16)(i << 8);
    g_tcmips_sdl.gamma_ramp[2 * 256 + i] = (Uint16)(i << 8);
  }
}
