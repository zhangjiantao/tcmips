/* TCMIPS-specific SDL extensions: hardware framebuffer query.
 *
 * libsdl drives the TCMIPS pixel console directly.  These extension helpers
 * let a caller that is integrated with libsdl read back the physical VRAM
 * that libsdl currently owns, e.g. in automated tests to verify that what
 * was painted on the logical surface actually landed in hardware.
 *
 * The query reports the actual hardware layout (resolution + pixel format),
 * so a caller never has to assume the VRAM is in some hard-coded format.
 *
 * Names use the reserved `tcmips_sdl_` prefix so they never collide with the
 * standard SDL 1.2 API / ABI.
 */

#ifndef _SDL_tcmips_h
#define _SDL_tcmips_h

/* Bring in the project-level build configuration first so that all SDL1
 * headers below can react to TCMIPS-specific build flags such as
 * TCM_SDL_DEBUG.  Having this #include on the most-leaf header guarantees
 * every SDL1 header that includes SDL_types.h (which is "include first" in
 * the chain) automatically inherits the flag without redeclaration. */
#include <tcm_config.h>

#include "SDL_types.h"
#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Description of the physical (hardware) VRAM pixel-array layout.
 * width/height are the hardware pixel dimensions; bits_per_pixel /
 * bytes_per_pixel give the storage size per pixel, and the channel masks
 * describe how an RGBA byte-array value is packed into that pixel (host
 * endianness).  The pixel value is 0xRRGGBBAA on this little-endian
 * target: R lives in the lowest byte (rmask == 0x000000FF) and A in the
 * highest (amask == 0xFF000000).  This matches SDL's RGBA32 format, NOT
 * ABGR8888 (SDL's ABGR8888 would have A in the lowest byte).
 */
typedef struct tcmips_sdl_vram_format {
    int     width;
    int     height;
    int     bits_per_pixel;
    int     bytes_per_pixel;
    Uint32  rmask;
    Uint32  gmask;
    Uint32  bmask;
    Uint32  amask;
} tcmips_sdl_vram_format_t;

/* Query the physical (hardware) VRAM framebuffer that libsdl currently owns
 * (it drives the TCMIPS pixel console directly).
 *
 * Returns the VRAM base pointer, or NULL while the pixel console is not
 * active.  The pointer is valid for reading (and writing) by the guest.
 * When @p fmt is non-NULL it receives the hardware layout (see struct above);
 * the per-row stride equals width * bytes_per_pixel. */
SDL_DECLSPEC void * SDLCALL tcmips_sdl_vram_info(tcmips_sdl_vram_format_t *fmt);

/* Runtime query for SDL1's internal leak-tracking counter.  Only visible
 * when the project-level TCM_SDL_DEBUG switch (in the root CMakeLists.txt)
 * is ON, which defines the `TCM_SDL_DEBUG` macro via tcm_config.h and
 * exports it to every consumer of `<SDL/...>`.  Returns the number of live
 * SDL_malloc/calloc/realloc/aligned_alloc allocations not yet matched by
 * an SDL_free, or zero if the layer was built without leak tracking.
 * Automated tests can call this after SDL_Quit() to detect leftover
 * allocations — see test_sdl1_autotest for the reference pattern. */
#ifdef TCM_SDL_DEBUG
SDL_DECLSPEC size_t SDLCALL tcmips_sdl_debug_alloc_count(void);
#endif

#ifdef __cplusplus
}
#endif

#include "close_code.h"

#endif /* _SDL_tcmips_h */