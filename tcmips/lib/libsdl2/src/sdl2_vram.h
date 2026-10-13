/* Internal VRAM driver for the TCMIPS SDL2 compatibility layer.
 *
 * This layer talks DIRECTLY to the TCMIPS hardware pixel console
 * (tcm_pixel_console_init in tcmips/dev), mirroring the SDL1 compatibility
 * layer (tcmips/lib/libsdl/src/sdl_vram.h).  The two layers keep an
 * identical interface and an identical rescale core so that both
 * compatibility layers behave consistently.
 *
 * The SDL2 layer owns a logical window surface (allocated by SDL_CreateWindow
 * at the user-requested size) and submits it to the physical VRAM framebuffer
 * returned by the platform at init time.
 *
 * SDL2 upstream headers from branch SDL2,
 * commit b90ac95029d801c5abc59472ba8e2200dff31e1e
 */

#ifndef TCMIPS_SDL2_VRAM_H
#define TCMIPS_SDL2_VRAM_H

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum logical resolution for SDL2 window surfaces.  The TCMIPS physical
 * console is much smaller, but the logical surface can be larger and is scaled
 * down on present.  1920x1080 (1080p) keeps the 32-bpp window buffer under ~8
 * MiB. */
#define TCMIPS_SDL2_MAX_LOGICAL_WIDTH 1920
#define TCMIPS_SDL2_MAX_LOGICAL_HEIGHT 1080

/* Initialize the hardware pixel console for a user-requested logical size.
 *
 * The TCMIPS 4:3 console derives its hardware resolution from the requested
 * width (rounded up to a multiple of 4).  The physical framebuffer base
 * address, physical width and physical height are cached in this module.
 *
 * Returns 0 on success, -1 on failure (invalid size / console init failed).
 */
int tcmips_sdl2_vram_init(int logical_w, int logical_h);

/* Submit the SDL2 logical window surface (RGBA32) to the physical VRAM.
 *
 * When logical and physical resolutions match, this is a plain per-row
 * memcpy (the hot path).  When they differ, a nearest-neighbour rescale maps
 * the logical buffer onto the whole physical screen, using the same column
 * map precomputation used by the SDL1 layer.
 *
 * pixels        : SDL_GetWindowSurface's pixels (logical buffer).
 * logical_w/h   : surface->w / surface->h.
 * logical_pitch : surface->pitch in bytes.
 */
void tcmips_sdl2_vram_submit(const void *pixels, int logical_w, int logical_h,
                             int logical_pitch);

/* Return the actual physical console resolution chosen at init. */
void tcmips_sdl2_vram_get_size(int *w, int *h);

/* The hardware VRAM layout type and query are shared with the public
 * <SDL2/SDL2_tcmips.h> extension (same reporting policy as the SDL1 layer).
 * This private header sits in src/, which is added to the include path as
 * ".../include/SDL2" equivalent, so the same-named public header is
 * resolvable without the SDL2/ prefix. */
#include "SDL2_tcmips.h"

/* Query the physical framebuffer this layer owns (direct driver pointer, same
 * origin policy as the SDL1 layer's tcmips_sdl_vram_info).  Returns the VRAM
 * base pointer or NULL when not initialized; when @p fmt is non-NULL it also
 * receives the hardware layout so callers need not assume a fixed format. */
void *tcmips_sdl2_vram_info(tcmips_sdl2_vram_format_t *fmt);

/* Tear down the pixel console (undoes vram_init). */
void tcmips_sdl2_vram_quit(void);

#ifdef __cplusplus
}
#endif

#endif /* TCMIPS_SDL2_VRAM_H */