/* TCMIPS-specific SDL2 extensions: hardware framebuffer query.
 *
 * Mirrors the SDL1 compatibility layer's tcmips_sdl_vram_info() extension
 * (tcmips/lib/libsdl/include/SDL/SDL_tcmips.h), so the two compatibility
 * layers expose the same "report the hardware VRAM layout" interface.
 *
 * The SDL2 layer drives the TCMIPS pixel console directly; the caller may
 * read (and write) the returned VRAM pointer to verify what actually landed
 * in hardware.  The layout struct reports resolution + pixel format so a
 * caller never has to assume a hard-coded VRAM format.
 *
 * Names use the reserved `tcmips_sdl2_` prefix so they never collide with the
 * standard SDL2 API / ABI.
 */

#ifndef _SDL2_tcmips_h
#define _SDL2_tcmips_h

/* Bring in the project-level build configuration first so the TCM_SDL_DEBUG
 * flag (controlled by the project-level TCM_SDL_DEBUG switch in the root
 * CMakeLists.txt, shared with libsdl) is visible to this header and
 * therefore to every consumer of the SDL2_tcmips extension interface.
 * The two SDL compatibility layers (libsdl / libsdl2) use the same macro
 * / switch name; the actual counter is exposed as
 * `tcmips_sdl2_debug_alloc_count()` below — only when the flag is defined. */
#include <tcm_config.h>

#include "SDL_platform.h"  /* for any platform-specific setup SDL.h needs */

#include "SDL.h"
#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Physical (hardware) VRAM pixel-array layout.  width/height are the hardware
 * pixel dimensions; bits/bytes_per_pixel give the storage size per pixel, and
 * the channel masks describe how an RGBA byte-array value is packed (host
 * endianness).  On this little-endian target the pixel value is
 * 0xRRGGBBAA, i.e. R lives in the lowest byte (rmask == 0x000000FF) and A in
 * the highest (amask == 0xFF000000).  This matches SDL's RGBA32 format, NOT
 * ABGR8888 (SDL's ABGR8888 would have A in the lowest byte). */
typedef struct tcmips_sdl2_vram_format {
    int     width;
    int     height;
    int     bits_per_pixel;
    int     bytes_per_pixel;
    Uint32  rmask;
    Uint32  gmask;
    Uint32  bmask;
    Uint32  amask;
} tcmips_sdl2_vram_format_t;

/* Query the physical (hardware) VRAM framebuffer this layer currently owns.
 * Returns the VRAM base pointer, or NULL while the pixel console is not
 * active.  When @p fmt is non-NULL it receives the hardware layout; the
 * per-row stride equals width * bytes_per_pixel.
 *
 * Plain linkage: this compat layer is a static library, so no export macro
 * is needed (matching the other tcmips_sdl2 internal declarations). */
void *tcmips_sdl2_vram_info(tcmips_sdl2_vram_format_t *fmt);

/* Runtime query for SDL2's internal leak-tracking counter.  Only visible
 * when the project-level TCM_SDL_DEBUG switch (in the root CMakeLists.txt)
 * is ON, which defines `TCM_SDL_DEBUG` via tcm_config.h and exports it to
 * every consumer of `<SDL2/...>`.  Returns the number of live
 * SDL_malloc/calloc/realloc/aligned_alloc allocations not yet matched by
 * an SDL_free, or zero if the layer was built without leak tracking.
 * Automated tests can call this after SDL_Quit() to detect leftover
 * allocations — see test_sdl2_autotest for the reference pattern. */
#ifdef TCM_SDL_DEBUG
size_t tcmips_sdl2_debug_alloc_count(void);
#endif

#ifdef __cplusplus
}
#endif

#include "close_code.h"

#endif /* _SDL2_tcmips_h */