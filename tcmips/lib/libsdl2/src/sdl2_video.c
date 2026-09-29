#include "sdl2_vram.h"
#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TCMIPS uses a fixed pixel format matching the hardware VRAM layout:
// byte 0 = R, byte 1 = G, byte 2 = B, byte 3 = A (unused, should be 0).
// On a little-endian system this is exactly SDL_PIXELFORMAT_RGBA32.
#define TCMIPS_SDL_FORMAT SDL_PIXELFORMAT_RGBA32
#define TCMIPS_SDL_DEPTH 32
#define TCMIPS_SDL_RMASK 0x000000FF
#define TCMIPS_SDL_GMASK 0x0000FF00
#define TCMIPS_SDL_BMASK 0x00FF0000
#define TCMIPS_SDL_AMASK 0xFF000000

// Dummy window IDs start at 1.
static Uint32 s_next_window_id = 1;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Initialize the hardware pixel console for a window of the user-requested
// size.  The physical framebuffer base address and the physical 4:3
// resolution are owned by tcmips_sdl2_vram (the SDL1 layer keeps an identical,
// independent driver).  We only mirror the physical size into win.w/h because
// SDL_GetCurrentDisplayMode / SDL_GetDesktopDisplayMode rely on it.
int tcmips_sdl2_init_video(int req_w, int req_h) {
  if (tcmips_sdl2_vram_init(req_w, req_h) != 0) {
    SDL_SetError("tcmips_sdl2_vram_init failed");
    return -1;
  }
  tcmips_sdl2_vram_get_size(&g_tcmips_sdl.win.w, &g_tcmips_sdl.win.h);
  return 0;
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h,
                             Uint32 flags) {
  (void)title;
  (void)x;
  (void)y;
  (void)flags;

  // TCMIPS is a single-window platform — reject a second call.
  if (g_tcmips_sdl.win.window) {
    SDL_SetError("TCMIPS supports only one window; must SDL_DestroyWindow "
                 "before re-creating");
    return NULL;
  }

  // Clamp to logical limits.  The physical console is much smaller; the
  // logical frame will be scaled down on present.
  if (w <= 0 || h <= 0 || w > TCMIPS_SDL2_MAX_LOGICAL_WIDTH ||
      h > TCMIPS_SDL2_MAX_LOGICAL_HEIGHT) {
    SDL_SetError("Invalid window size %dx%d (max %dx%d)", w, h,
                 TCMIPS_SDL2_MAX_LOGICAL_WIDTH, TCMIPS_SDL2_MAX_LOGICAL_HEIGHT);
    return NULL;
  }

  SDL_Window *win = (SDL_Window *)SDL_calloc(1, sizeof(SDL_Window));
  if (!win) {
    SDL_OutOfMemory();
    return NULL;
  }

  win->id = s_next_window_id++;
  win->w = w;
  win->h = h;
  win->flags = flags;

  g_tcmips_sdl.win.window = win;
  g_tcmips_sdl.win.flags = flags;

  // Ask the VRAM driver to bring the hardware framebuffer up at the
  // closest 4:3 step-rounded resolution to the user-requested size.  The
  // physical framebuffer address and pitch live inside the VRAM driver.
  if (tcmips_sdl2_init_video(w, h) != 0) {
    SDL_free(win);
    g_tcmips_sdl.win.window = NULL;
    SDL_SetError("tcmips_sdl2_init_video failed");
    return NULL;
  }

  // Create a public surface of the user-requested size tied to this window.
  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, TCMIPS_SDL_DEPTH,
                                                     TCMIPS_SDL_FORMAT);
  if (!surf) {
    SDL_free(win);
    g_tcmips_sdl.win.window = NULL;
    tcmips_sdl2_vram_quit();
    return NULL;
  }
  g_tcmips_sdl.win.surface = surf;

  return win;
}

SDL_Window *SDL_CreateWindowFrom(const void *data) {
  (void)data;
  return NULL;
}

void SDL_DestroyWindow(SDL_Window *window) {
  if (!window)
    return;
  // Only the currently registered single window can be destroyed.
  if (g_tcmips_sdl.win.window != window) {
    SDL_SetError("SDL_DestroyWindow: window is not the active TCMIPS window");
    return;
  }

  if (g_tcmips_sdl.win.surface) {
    SDL_FreeSurface(g_tcmips_sdl.win.surface);
    g_tcmips_sdl.win.surface = NULL;
  }
  // Tear down the hardware pixel console that vram_init brought up for this
  // window.  A subsequent SDL_CreateWindow will re-init it via
  // tcmips_sdl2_init_video().
  tcmips_sdl2_vram_quit();
  g_tcmips_sdl.win.window = NULL;
  SDL_free(window);
}

const char *SDL_GetWindowTitle(SDL_Window *window) {
  (void)window;
  return "";
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title) {
  (void)window;
  (void)title;
}

void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
  (void)window;
  (void)icon;
}

void *SDL_SetWindowData(SDL_Window *window, const char *name, void *userdata) {
  (void)window;
  (void)name;
  (void)userdata;
  return NULL;
}

void *SDL_GetWindowData(SDL_Window *window, const char *name) {
  (void)window;
  (void)name;
  return NULL;
}

Uint32 SDL_GetWindowID(SDL_Window *window) {
  if (!window)
    return 0;
  return window->id;
}

SDL_Window *SDL_GetWindowFromID(Uint32 id) {
  if (g_tcmips_sdl.win.window && g_tcmips_sdl.win.window->id == id) {
    return g_tcmips_sdl.win.window;
  }
  return NULL;
}

Uint32 SDL_GetWindowFlags(SDL_Window *window) {
  if (!window)
    return 0;
  return window->flags;
}

void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y) {
  (void)window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
  if (!window) {
    if (w)
      *w = 0;
    if (h)
      *h = 0;
    return;
  }
  if (w)
    *w = window->w;
  if (h)
    *h = window->h;
}

void SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h) {
  SDL_GetWindowSize(window, w, h);
}

int SDL_GetWindowBordersSize(SDL_Window *window, int *top, int *left,
                             int *bottom, int *right) {
  (void)window;
  if (top)
    *top = 0;
  if (left)
    *left = 0;
  if (bottom)
    *bottom = 0;
  if (right)
    *right = 0;
  return 0;
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y) {
  (void)window;
  (void)x;
  (void)y;
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h) {
  (void)window;
  (void)w;
  (void)h;
}

void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h) {
  (void)window;
  (void)min_w;
  (void)min_h;
  /* Upstream records the minimum in the window and lets the WM resize; the
   * TCMIPS window is fixed-size, so this must NOT touch window->w/h — the
   * window size has to stay consistent with the backing window surface
   * (SDL_GetWindowSize vs SDL_GetWindowSurface). */
}

void SDL_GetWindowMinimumSize(SDL_Window *window, int *w, int *h) {
  SDL_GetWindowSize(window, w, h);
}

void SDL_SetWindowMaximumSize(SDL_Window *window, int max_w, int max_h) {
  (void)window;
  (void)max_w;
  (void)max_h;
}

void SDL_GetWindowMaximumSize(SDL_Window *window, int *w, int *h) {
  SDL_GetWindowSize(window, w, h);
}

void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered) {
  (void)window;
  (void)bordered;
}

void SDL_SetWindowResizable(SDL_Window *window, SDL_bool resizable) {
  (void)window;
  (void)resizable;
}

void SDL_SetWindowAlwaysOnTop(SDL_Window *window, SDL_bool on_top) {
  (void)window;
  (void)on_top;
}

int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {
  (void)window;
  (void)flags;
  return 0;
}

void SDL_SetWindowGrab(SDL_Window *window, SDL_bool grabbed) {
  (void)window;
  (void)grabbed;
}

SDL_bool SDL_GetWindowGrab(SDL_Window *window) {
  (void)window;
  return SDL_FALSE;
}

void SDL_SetWindowMouseGrab(SDL_Window *window, SDL_bool grabbed) {
  (void)window;
  (void)grabbed;
}

SDL_bool SDL_GetWindowMouseGrab(SDL_Window *window) { return SDL_FALSE; }

void SDL_SetWindowKeyboardGrab(SDL_Window *window, SDL_bool grabbed) {
  (void)window;
  (void)grabbed;
}

SDL_bool SDL_GetWindowKeyboardGrab(SDL_Window *window) { return SDL_FALSE; }

int SDL_SetWindowGammaRamp(SDL_Window *window, const Uint16 *r, const Uint16 *g,
                           const Uint16 *b) {
  (void)window;
  (void)r;
  (void)g;
  (void)b;
  return 0;
}

int SDL_GetWindowGammaRamp(SDL_Window *window, Uint16 *r, Uint16 *g,
                           Uint16 *b) {
  (void)window;
  (void)r;
  (void)g;
  (void)b;
  return -1;
}

int SDL_SetWindowHitTest(SDL_Window *window, SDL_HitTest callback,
                         void *callback_data) {
  (void)window;
  (void)callback;
  (void)callback_data;
  return -1;
}

void SDL_ShowWindow(SDL_Window *window) { (void)window; }

void SDL_HideWindow(SDL_Window *window) { (void)window; }

void SDL_RaiseWindow(SDL_Window *window) { (void)window; }

void SDL_MaximizeWindow(SDL_Window *window) { (void)window; }

void SDL_MinimizeWindow(SDL_Window *window) { (void)window; }

void SDL_RestoreWindow(SDL_Window *window) { (void)window; }

void SDL_DisableScreenSaver(void) {}
void SDL_EnableScreenSaver(void) {}
SDL_bool SDL_IsScreenSaverEnabled(void) { return SDL_FALSE; }

int SDL_UpdateWindowSurface(SDL_Window *window) {
  if (window && g_tcmips_sdl.win.surface && g_tcmips_sdl.win.window == window) {
    tcmips_sdl2_present_framebuffer();
    return 0;
  }
  return -1;
}

int SDL_UpdateWindowSurfaceRects(SDL_Window *window, const SDL_Rect *rects,
                                 int numrects) {
  (void)rects;
  (void)numrects;
  return SDL_UpdateWindowSurface(window);
}

SDL_Surface *SDL_GetWindowSurface(SDL_Window *window) {
  if (!window)
    return NULL;
  return g_tcmips_sdl.win.surface;
}

// Display / screen info stubs.
int SDL_VideoInit(const char *driver_name) {
  (void)driver_name;
  return 0;
}

void SDL_VideoQuit(void) {}

int SDL_GetNumVideoDrivers(void) { return 1; }

const char *SDL_GetVideoDriver(int index) {
  (void)index;
  return "tcmips";
}

const char *SDL_GetCurrentVideoDriver(void) { return "tcmips"; }

int SDL_GetNumDisplays(void) { return 1; }

const char *SDL_GetDisplayName(int displayIndex) {
  (void)displayIndex;
  return "TCMIPS Display";
}

int SDL_GetDisplayBounds(int displayIndex, SDL_Rect *rect) {
  if (rect) {
    rect->x = 0;
    rect->y = 0;
    rect->w = g_tcmips_sdl.win.w;
    rect->h = g_tcmips_sdl.win.h;
  }
  (void)displayIndex;
  return 0;
}

int SDL_GetDisplayUsableBounds(int displayIndex, SDL_Rect *rect) {
  return SDL_GetDisplayBounds(displayIndex, rect);
}

int SDL_GetDisplayDPI(int displayIndex, float *ddpi, float *hdpi, float *vdpi) {
  (void)displayIndex;
  if (ddpi)
    *ddpi = 96.0f;
  if (hdpi)
    *hdpi = 96.0f;
  if (vdpi)
    *vdpi = 96.0f;
  return 0;
}

SDL_DisplayOrientation SDL_GetDisplayOrientation(int displayIndex) {
  (void)displayIndex;
  return SDL_ORIENTATION_LANDSCAPE;
}

int SDL_GetNumDisplayModes(int displayIndex) {
  (void)displayIndex;
  return 1;
}

int SDL_GetDisplayMode(int displayIndex, int modeIndex, SDL_DisplayMode *mode) {
  (void)displayIndex;
  (void)modeIndex;
  if (mode) {
    mode->w = g_tcmips_sdl.win.w;
    mode->h = g_tcmips_sdl.win.h;
    mode->refresh_rate = 60;
    mode->format = TCMIPS_SDL_FORMAT;
  }
  return 0;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
  return SDL_GetDisplayMode(displayIndex, 0, mode);
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
  return SDL_GetDisplayMode(displayIndex, 0, mode);
}

SDL_DisplayMode *SDL_GetClosestDisplayMode(int displayIndex,
                                           const SDL_DisplayMode *mode,
                                           SDL_DisplayMode *closest) {
  (void)displayIndex;
  if (closest && mode) {
    *closest = *mode;
    closest->format = TCMIPS_SDL_FORMAT;
  }
  return closest;
}

int SDL_GetWindowDisplayIndex(SDL_Window *window) {
  (void)window;
  return 0;
}

int SDL_GetWindowDisplayMode(SDL_Window *window, SDL_DisplayMode *mode) {
  if (!window)
    return -1;
  if (mode) {
    mode->w = window->w;
    mode->h = window->h;
    mode->refresh_rate = 60;
    mode->format = TCMIPS_SDL_FORMAT;
  }
  return 0;
}

int SDL_SetWindowDisplayMode(SDL_Window *window, const SDL_DisplayMode *mode) {
  (void)window;
  (void)mode;
  return 0;
}

int SDL_GetWindowOpacity(SDL_Window *window, float *out_opacity) {
  (void)window;
  if (out_opacity)
    *out_opacity = 1.0f;
  return 0;
}

int SDL_SetWindowOpacity(SDL_Window *window, float opacity) {
  (void)window;
  (void)opacity;
  return 0;
}

int SDL_SetWindowBrightness(SDL_Window *window, float brightness) {
  (void)window;
  (void)brightness;
  return 0;
}

float SDL_GetWindowBrightness(SDL_Window *window) {
  (void)window;
  return 1.0f;
}

int SDL_SetWindowModalFor(SDL_Window *modal_window, SDL_Window *parent_window) {
  (void)modal_window;
  (void)parent_window;
  return -1;
}

int SDL_SetWindowInputFocus(SDL_Window *window) {
  (void)window;
  return 0;
}

int SDL_FlashWindow(SDL_Window *window, SDL_FlashOperation operation) {
  (void)window;
  (void)operation;
  return -1;
}

Uint32 SDL_GetWindowPixelFormat(SDL_Window *window) {
  (void)window;
  return TCMIPS_SDL_FORMAT;
}

// Message boxes are not supported; just log.
int SDL_ShowMessageBox(const SDL_MessageBoxData *messageboxdata,
                       int *buttonid) {
  (void)buttonid;
  if (messageboxdata && messageboxdata->message) {
    SDL_Log("[TCMIPS SDL] MessageBox: %s", messageboxdata->message);
  }
  return -1;
}

int SDL_ShowSimpleMessageBox(Uint32 flags, const char *title,
                             const char *message, SDL_Window *window) {
  (void)flags;
  (void)title;
  (void)window;
  SDL_Log("[TCMIPS SDL] %s: %s", title ? title : "Message",
          message ? message : "");
  return -1;
}

// ---------------------------------------------------------------------------
// Mouse / cursor
// ---------------------------------------------------------------------------

Uint32 SDL_GetMouseState(int *x, int *y) {
  if (x)
    *x = 0;
  if (y)
    *y = 0;
  return 0;
}

Uint32 SDL_GetGlobalMouseState(int *x, int *y) {
  return SDL_GetMouseState(x, y);
}

Uint32 SDL_GetRelativeMouseState(int *x, int *y) {
  return SDL_GetMouseState(x, y);
}

void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y) {
  (void)window;
  (void)x;
  (void)y;
}

int SDL_SetRelativeMouseMode(SDL_bool enabled) {
  (void)enabled;
  return -1;
}

SDL_bool SDL_GetRelativeMouseMode(void) { return SDL_FALSE; }

int SDL_CaptureMouse(SDL_bool enabled) {
  (void)enabled;
  return 0;
}

SDL_Cursor *SDL_CreateCursor(const Uint8 *data, const Uint8 *mask, int w, int h,
                             int hot_x, int hot_y) {
  (void)data;
  (void)mask;
  (void)w;
  (void)h;
  (void)hot_x;
  (void)hot_y;
  TCMIPS_SDL_STUB_MSG("SDL_CreateCursor");
  return NULL;
}

SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *surface, int hot_x, int hot_y) {
  (void)surface;
  (void)hot_x;
  (void)hot_y;
  TCMIPS_SDL_STUB_MSG("SDL_CreateColorCursor");
  return NULL;
}

SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id) {
  (void)id;
  TCMIPS_SDL_STUB_MSG("SDL_CreateSystemCursor");
  return NULL;
}

SDL_Cursor *SDL_GetDefaultCursor(void) { return g_tcmips_sdl.active_cursor; }

void SDL_SetCursor(SDL_Cursor *cursor) { g_tcmips_sdl.active_cursor = cursor; }

SDL_Cursor *SDL_GetCursor(void) { return g_tcmips_sdl.active_cursor; }

void SDL_FreeCursor(SDL_Cursor *cursor) {
  if (cursor && cursor != g_tcmips_sdl.active_cursor) {
    SDL_free(cursor);
  }
}

int SDL_ShowCursor(int toggle) {
  (void)toggle;
  return 0;
}

// ---------------------------------------------------------------------------
// Hardware framebuffer flush
// ---------------------------------------------------------------------------

void tcmips_sdl2_present_framebuffer(void) {
  SDL_Surface *surf = g_tcmips_sdl.win.surface;
  if (!surf || !surf->pixels)
    return;

  // The VRAM driver owns the physical console size/address and performs a
  // 1:1 copy when dimensions match or a nearest-neighbour rescale onto the
  // whole physical screen otherwise (same core as the SDL1 layer).
  tcmips_sdl2_vram_submit(surf->pixels, surf->w, surf->h, surf->pitch);
}
