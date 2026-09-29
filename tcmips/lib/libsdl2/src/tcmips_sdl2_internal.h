// Internal helper declarations for the TCMIPS SDL2 compatibility layer.
// Not part of the public SDL2 API.

#ifndef TCMIPS_SDL2_INTERNAL_H
#define TCMIPS_SDL2_INTERNAL_H

#include <SDL.h>
#include "sdl2_vram.h"

#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCMIPS_SDL_EVENT_QUEUE_SIZE 64
#define TCMIPS_SDL_MAX_KEY_STATES   SDL_NUM_SCANCODES

// Per-call stub logging, disabled by default in release/during inner loops.
#ifndef TCMIPS_SDL_STUB_LOG_ENABLED
#define TCMIPS_SDL_STUB_LOG_ENABLED 1
#endif

#if TCMIPS_SDL_STUB_LOG_ENABLED
#include <stdio.h>
#define TCMIPS_SDL_STUB_MSG(name) do {                         \
    static int __logged = 0;                                     \
    if (!__logged) {                                             \
        __logged = 1;                                            \
        SDL_Log("[SDL_STUB] %s", (name));                        \
        fprintf(stdout, "[SDL_STUB] %s\n", (name));              \
        fflush(stdout);                                          \
    }                                                            \
} while (0)
#else
#define TCMIPS_SDL_STUB_MSG(name) do {} while (0)
#endif

// Global state; kept in a single struct to make relocation into the repository easier.

// Opaque SDL_Window is only forward-declared in SDL_video.h; complete it here.
struct SDL_Window {
    Uint32 id;
    int w;
    int h;
    Uint32 flags;
};

typedef struct {
    bool initialized;
    Uint32 init_ticks;
    uint64_t init_unix_ms;

    // Event queue.
    SDL_Event  evt_queue[TCMIPS_SDL_EVENT_QUEUE_SIZE];
    volatile uint32_t evt_head;
    volatile uint32_t evt_tail;

    // Keyboard state for SDL_GetKeyboardState.
    Uint8 key_state[SDL_NUM_SCANCODES];

    // Single hardware-backed window.
    struct {
        struct SDL_Window *window;
        // Physical console resolution (mirrored from the VRAM driver;
        // used for display mode queries).
        int w;
        int h;
        Uint32 flags;
        SDL_Surface *surface;
    } win;

    // Software renderer built on top of the window surface.
    struct {
        SDL_Renderer *renderer;
        SDL_Texture  *target;
        SDL_Rect      viewport;
        SDL_bool      clip_enabled;
        SDL_Rect      clip;
        SDL_bool      integer_scale;
        float         scale_x;
        float         scale_y;
        SDL_BlendMode blend;
        Uint8         r, g, b, a;
        SDL_Vertex     blank_vertex; // unused, avoids empty struct
    } render;

    // Active mouse cursor (unused, but required to avoid NULL).
    SDL_Cursor *active_cursor;
} tcmips_sdl2_t;

extern tcmips_sdl2_t g_tcmips_sdl;

// Helpers implemented in separate .c files.
uint64_t tcmips_sdl2_unix_ms_now(void);

// Reset ALL global state and re-timestamp the tick base — called by
// SDL_Init so the caller always gets a clean slate (mirrors the SDL1
// layer's tcmips_sdl_init_state).
void tcmips_sdl2_init_state(void);

// RGBA32 <-> channel unpack helpers (shared with the renderer).
void    tcmips_sdl2_pixel_to_rgba(Uint32 pixel, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);
Uint32  tcmips_sdl2_rgba_to_pixel(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

// Event handling.
void tcmips_sdl2_poll_hardware(void);
// Tail-push an event; returns SDL_TRUE when stored, SDL_FALSE when the queue
// was full (so SDL_PushEvent can honour its return contract).
SDL_bool tcmips_sdl2_post_event(const SDL_Event *e);

// Key mapping.
SDL_Scancode tcmips_keycode_to_scancode(uint16_t tcm_code);

// Build a read/write surface that aliases caller-owned memory (upstream
// SDL_CreateRGBSurfaceFrom semantics; flagged PREALLOC|DONTFREE so
// SDL_FreeSurface never releases `pixels`).  Used by
// SDL_CreateRGBSurfaceFrom/WithFormatFrom and SDL_LockTextureToSurface.
SDL_Surface *tcmips_sdl2_surface_alias_rgba(void *pixels, int width, int height,
                                            int pitch);

// Video backend helpers.
//   Initialize the hardware pixel console to the hardware resolution that maps
//   best to the requested (req_w, req_h).  Caches the framebuffer base address
//   and pitch returned by the platform init interface in g_tcmips_sdl.win.
//   Returns 0 on success, -1 on failure.
int tcmips_sdl2_init_video(int req_w, int req_h);
void tcmips_sdl2_present_framebuffer(void);

// Memory leak detection (only when TCM_SDL_DEBUG is defined at compile
// time).  Returns the number of SDL_malloc/calloc/realloc allocations
// that have not yet been matched by an SDL_free, or zero if tracking is
// disabled.
#ifdef TCM_SDL_DEBUG
size_t tcmips_sdl2_debug_alloc_count(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // TCMIPS_SDL2_INTERNAL_H
