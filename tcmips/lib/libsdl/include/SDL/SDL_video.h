/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_video.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_video_h
#define _SDL_video_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* This is the structure for ports / window positions (SDL1 uses int). */
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000u

/* OpenGL attributes for SDL_GL_GetAttribute / SDL_GL_SetAttribute.
   TCMIPS does not support OpenGL; these are kept for API compatibility. */
typedef enum {
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_STENCIL_SIZE,
    SDL_GL_ACCUM_RED_SIZE,
    SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE,
    SDL_GL_ACCUM_ALPHA_SIZE,
    SDL_GL_STEREO,
    SDL_GL_MULTISAMPLEBUFFERS,
    SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL,
    SDL_GL_SWAP_CONTROL,
    SDL_GL_MAX
} SDL_GLattr;

/* Mouse input grab mode. */
typedef enum {
    SDL_GRAB_QUERY = -1,
    SDL_GRAB_OFF = 0,
    SDL_GRAB_ON = 1,
    SDL_GRAB_FULLSCREEN
} SDL_GrabMode;

/* Useful data types */
typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

typedef struct SDL_Color {
    union {
        struct {
            Uint8 r;
            Uint8 g;
            Uint8 b;
            Uint8 unused;
        };
        Uint32 val;
    };
} SDL_Color;

#define SDL_ALPHA_OPAQUE 255
#define SDL_ALPHA_TRANSPARENT 0

/* Everything in the pixel format structure is read-only */
typedef struct SDL_Palette {
    int       ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette *palette;
    Uint8  BitsPerPixel;
    Uint8  BytesPerPixel;
    Uint8  Rloss;
    Uint8  Gloss;
    Uint8  Bloss;
    Uint8  Aloss;
    Uint8  Rshift;
    Uint8  Gshift;
    Uint8  Bshift;
    Uint8  Ashift;
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    Uint32 colorkey;
    Uint8  alpha;
} SDL_PixelFormat;

/* This structure should be treated as read-only, except for 'pixels',
   which, if not NULL, contains the raw pixel data for the surface. */
typedef struct SDL_Surface {
    Uint32 flags;                           /* Read-only */
    SDL_PixelFormat *format;                /* Read-only */
    int w, h;                               /* Read-only */
    Uint16 pitch;                           /* Read-only */
    void *pixels;                           /* Read-write */
    int offset;                             /* Private */

    /* Hardware-specific surface info -- not used by TCMIPS */
    void *hwdata;

    /* clipping information */
    SDL_Rect clip_rect;                     /* Read-only */

    /* Reference count -- used when freeing the surface */
    int refcount;                           /* Read-mostly */

    /* ? */
    struct SDL_Surface *map;                /* Private */

    /* format version, NULL -> create in progress */
    unsigned int format_version;            /* Private */

    /* surface lock count.  Access to pixels is allowed when this is 0. */
    int locked;                             /* Private */

    /* information for accelerated blits */
    struct SDL_BlitMap *map_alloc;
} SDL_Surface;

/* Cursor type for SDL_ShowCursor / SDL_SetCursor (TCMIPS has no real cursor). */
typedef struct SDL_Cursor { int unused; } SDL_Cursor;

SDL_DECLSPEC int SDLCALL SDL_ShowCursor(int toggle);
SDL_DECLSPEC SDL_Cursor * SDLCALL SDL_CreateCursor(Uint8 *data, Uint8 *mask,
                                                   int w, int h, int hot_x, int hot_y);
SDL_DECLSPEC void SDLCALL SDL_SetCursor(SDL_Cursor *cursor);
SDL_DECLSPEC SDL_Cursor * SDLCALL SDL_GetCursor(void);
SDL_DECLSPEC void SDLCALL SDL_FreeCursor(SDL_Cursor *cursor);

SDL_DECLSPEC Uint8 SDLCALL SDL_GetMouseState(int *x, int *y);
SDL_DECLSPEC Uint8 SDLCALL SDL_GetRelativeMouseState(int *x, int *y);
SDL_DECLSPEC void SDLCALL SDL_WarpMouse(Uint16 x, Uint16 y);

/* flags for SDL_Surface */
#define SDL_SWSURFACE	0x00000000	/* Surface is in system memory */
#define SDL_HWSURFACE	0x00000001	/* Surface is in video memory */
#define SDL_ASYNCBLIT	0x00000004	/* Use asynchronous blits if possible */
/* Available for SDL_SetVideoMode() */
#define SDL_ANYFORMAT	0x10000000
#define SDL_HWPALETTE	0x20000000
#define SDL_DOUBLEBUF	0x40000000
#define SDL_FULLSCREEN	0x80000000
#define SDL_OPENGL      0x00000002
#define SDL_OPENGLBLIT	0x0000000A
#define SDL_RESIZABLE	0x00000010
#define SDL_NOFRAME     0x00000020
/* Internal use only */
#define SDL_HWACCEL     0x00000100
#define SDL_SRCCOLORKEY	0x00001000
#define SDL_RLEACCELOK	0x00002000
#define SDL_RLEACCEL    0x00004000
#define SDL_SRCALPHA    0x00010000
#define SDL_PREALLOC    0x01000000

/* flags for SDL_SetPalette() */
#define SDL_LOGPAL 0x01
#define SDL_PHYSPAL 0x02

/* typedef for blit function groups. */
struct SDL_BlitMap;
typedef int (SDLCALL *SDL_blit)(struct SDL_Surface *src, SDL_Rect *srcrect,
                                struct SDL_Surface *dst, SDL_Rect *dstrect);

/* TCMIPS surfaces are always software and never require locking, so the
   SDL_MUSTLOCK macro is a constant 0. SDL_MustLock() is still exported. */
#define SDL_MUSTLOCK(surface)	0

/* This function is called internally in the SDL library to sync up a surface
 * with its private video-memory representation. It is exported for testing.
 */
SDL_DECLSPEC int SDLCALL SDL_MustLock(SDL_Surface *surface);

/* video functions */
SDL_DECLSPEC int SDLCALL SDL_VideoInit(const char *driver_name, Uint32 flags);
SDL_DECLSPEC void SDLCALL SDL_VideoQuit(void);
SDL_DECLSPEC char * SDLCALL SDL_VideoDriverName(char *namebuf, int maxlen);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_GetVideoSurface(void);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags);
SDL_DECLSPEC int SDLCALL SDL_VideoModeOK(int width, int height, int bpp, Uint32 flags);
SDL_DECLSPEC SDL_Rect ** SDLCALL SDL_ListModes(SDL_PixelFormat *format, Uint32 flags);
SDL_DECLSPEC int SDLCALL SDL_LockSurface(SDL_Surface *surface);
SDL_DECLSPEC void SDLCALL SDL_UnlockSurface(SDL_Surface *surface);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                     Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
                                         int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_DECLSPEC void SDLCALL SDL_FreeSurface(SDL_Surface *surface);
SDL_DECLSPEC int SDLCALL SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
SDL_DECLSPEC int SDLCALL SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect,
                                          SDL_Surface *dst, SDL_Rect *dstrect);
SDL_DECLSPEC int SDLCALL SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect,
                                         SDL_Surface *dst, SDL_Rect *dstrect);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_ConvertSurface(SDL_Surface *src,
                                     SDL_PixelFormat *fmt, Uint32 flags);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_DisplayFormat(SDL_Surface *surface);
SDL_DECLSPEC SDL_Surface * SDLCALL SDL_DisplayFormatAlpha(SDL_Surface *surface);

SDL_DECLSPEC Uint32 SDLCALL SDL_MapRGB(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b);
SDL_DECLSPEC Uint32 SDLCALL SDL_MapRGBA(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
SDL_DECLSPEC void SDLCALL SDL_GetRGB(Uint32 pixel, SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b);
SDL_DECLSPEC void SDLCALL SDL_GetRGBA(Uint32 pixel, SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

SDL_DECLSPEC int SDLCALL SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key);
SDL_DECLSPEC int SDLCALL SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha);
SDL_DECLSPEC int SDLCALL SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect);
SDL_DECLSPEC void SDLCALL SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect);

SDL_DECLSPEC int SDLCALL SDL_SetPalette(SDL_Surface *surface, int flags,
                                         const SDL_Color *colors,
                                         int firstcolor, int ncolors);
SDL_DECLSPEC int SDLCALL SDL_SetColors(SDL_Surface *surface, SDL_Color *colors,
                                        int firstcolor, int ncolors);

/* screen update functions */
SDL_DECLSPEC int SDLCALL SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
SDL_DECLSPEC int SDLCALL SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects);
SDL_DECLSPEC int SDLCALL SDL_Flip(SDL_Surface *screen);

/* WM functions */
SDL_DECLSPEC void SDLCALL SDL_WM_SetCaption(const char *title, const char *icon);
SDL_DECLSPEC void SDLCALL SDL_WM_GetCaption(char **title, char **icon);
SDL_DECLSPEC void SDLCALL SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);
SDL_DECLSPEC int SDLCALL SDL_WM_IconifyWindow(void);
SDL_DECLSPEC int SDLCALL SDL_WM_ToggleFullScreen(SDL_Surface *surface);
SDL_DECLSPEC SDL_GrabMode SDLCALL SDL_WM_GrabInput(SDL_GrabMode mode);

/* OpenGL functions -- not implemented on TCMIPS */
SDL_DECLSPEC int SDLCALL SDL_GL_LoadLibrary(const char *path);
SDL_DECLSPEC void * SDLCALL SDL_GL_GetProcAddress(const char *proc);
SDL_DECLSPEC int SDLCALL SDL_GL_GetAttribute(SDL_GLattr attr, int *value);
SDL_DECLSPEC int SDLCALL SDL_GL_SetAttribute(SDL_GLattr attr, int value);
SDL_DECLSPEC void SDLCALL SDL_GL_SwapBuffers(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_video_h */
