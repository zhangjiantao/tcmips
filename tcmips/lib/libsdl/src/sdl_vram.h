/* Internal VRAM driver for the TCMIPS SDL compatibility layer.
 *
 * This layer talks DIRECTLY to the TCMIPS hardware pixel console
 * (tcm_pixel_console_init in tcmips/dev), NOT through the SDL2 layer.  It
 * owns a logical SDL1 framebuffer (allocated by SDL_SetVideoMode) and
 * submits it to the physical VRAM framebuffer returned by the platform.
 *
 * Upstream SDL-1.2 baseline: release-1.2.15,
 * commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
 */

#ifndef TCMIPS_SDL_VRAM_H
#define TCMIPS_SDL_VRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the hardware pixel console for a user-requested logical size.
 *
 * The TCMIPS 4:3 console derives its hardware resolution from the requested
 * width (rounded up to a multiple of 4).  The physical framebuffer base
 * address, physical width and physical height are cached in this module.
 *
 * Returns 0 on success, -1 on failure (invalid size / console init failed).
 */
int tcmips_sdl_vram_init(int *req_w, int *req_h);

/* Submit the SDL1 logical framebuffer to the physical VRAM.
 *
 * SDL_SetVideoMode pre-clamps the logical surface to the hardware 4:3
 * resolution, so the submit is a straight 1:1 conversion: identical 32-bpp
 * ABGR layouts with tightly packed pitch take a single full-screen memcpy
 * hot path, any other supported layout is converted per pixel.  (A
 * nearest-neighbour rescale submit with a precomputed column index map
 * exists in the SDL2 layer, tcmips/lib/libsdl2/src/sdl2_vram.c, which keeps
 * the user-requested logical size instead of pre-clamping.)
 */
void tcmips_sdl_vram_submit(SDL_Surface *screen);

/* Query the physical framebuffer that libsdl currently owns (submit target).
 *
 * Returns the VRAM base pointer, or NULL when the pixel console has not been
 * initialized.  The caller may read (and write) this memory to verify what
 * actually landed in hardware VRAM, without re-initialising the console.
 * When @p fmt is non-NULL it receives the hardware resolution and pixel
 * layout (channel masks), so callers need not assume a fixed format. */
#include <SDL/SDL_tcmips.h>
void *tcmips_sdl_vram_info(tcmips_sdl_vram_format_t *fmt);

/* Tear down the pixel console (undoes vram_init). */
void tcmips_sdl_vram_quit(void);

#ifdef __cplusplus
}
#endif

#endif /* TCMIPS_SDL_VRAM_H */