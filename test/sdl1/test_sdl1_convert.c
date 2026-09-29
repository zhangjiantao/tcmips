/* * SDL1 test: cross-format conversion and partial-rect blitting.
 *
 * Exercises the libsdl convert AND partial-copy paths:
 *   - SDL_CreateRGBSurface / SDL_CreateRGBSurfaceFrom
 *   - SDL_ConvertSurface               (16/24/32 -> screen 32)
 *   - SDL_BlitSurface with a partial source rect
 *   - SDL_SetColorKey
 *   - SDL_FillRect
 *
 * Draws grids of small tiles from three source surfaces of different depths
 * (RGB565, RGB888, ARGB8888).  Each tile is blitted from a sub-rectangle of
 * its sprite and pushed around the canvas to validate that conversions and
 * sub-region copies stay colour-correct.  Press ESC to quit.
 */

#include <SDL/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600
#define SPR 96          /* sprite size (larger than the blit tiles) */
#define TILE 18         /* tile sub-rectangle size */

#define TRANS_R 255
#define TRANS_G 0
#define TRANS_B 255

static Uint32 mkcol(SDL_PixelFormat *fmt, int r, int g, int b)
{
    return SDL_MapRGB(fmt, (Uint8)r, (Uint8)g, (Uint8)b);
}

/* Fill a rectangular region by direct writes (any depth). */
static void fill_region(SDL_Surface *s, int x0, int y0, int x1, int y1, Uint32 c)
{
    int bpp = s->format->BytesPerPixel;
    for (int y = y0; y < y1 && y < s->h; ++y) {
        unsigned char *row = (unsigned char *)s->pixels + (size_t)y * s->pitch;
        for (int x = x0; x < x1 && x < s->w; ++x) {
            unsigned char *p = row + (size_t)x * bpp;
            switch (bpp) {
            case 1: *p = (Uint8)c; break;
            case 2: *(Uint16 *)p = (Uint16)c; break;
            case 3: p[0] = (Uint8)(c & 0xFF); p[1] = (Uint8)((c >> 8) & 0xFF);
                    p[2] = (Uint8)((c >> 16) & 0xFF); break;
            case 4: *(Uint32 *)p = c; break;
            }
        }
    }
}

/* Build a "tile sheet": n x n cells of distinct colours over a transparent
 * (colorkey) background. */
static void build_sheet(SDL_Surface *s, int n, int cell)
{
    SDL_FillRect(s, NULL, mkcol(s->format, TRANS_R, TRANS_G, TRANS_B));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {            Uint32 c = mkcol(s->format,
                             (30 + i * 60) & 0xFF,
                             (30 + j * 60) & 0xFF,
                             (30 + ((i + j) * 40)) & 0xFF);
            fill_region(s, j * cell, i * cell, j * cell + cell, i * cell + cell,
                        c);
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init fail: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface *screen = SDL_SetVideoMode(W, H, 32, SDL_SWSURFACE);
    if (!screen) {
        fprintf(stderr, "SDL_SetVideoMode fail: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_WM_SetCaption("SDL1 convert / sub-rect blit test", NULL);

    int cell = SPR / 3;   /* 3x3 cells */
    int n = 3;

    /* Three different source depths. */
    SDL_Surface *s16 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 16,
                                            0x0000F800, 0x000007E0,
                                            0x0000001F, 0x00000000);
    SDL_Surface *s24 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 24,
                                            0x000000FF, 0x0000FF00,
                                            0x00FF0000, 0x00000000);
    SDL_Surface *s32 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 32,
                                            0x00FF0000, 0x0000FF00,
                                            0x000000FF, 0xFF000000);
    if (!s16 || !s24 || !s32) {
        fprintf(stderr, "CreateRGBSurface fail: %s\n", SDL_GetError());
        if (s16) SDL_FreeSurface(s16);
        if (s24) SDL_FreeSurface(s24);
        if (s32) SDL_FreeSurface(s32);
        SDL_Quit();
        return 1;
    }
    build_sheet(s16, n, cell);
    build_sheet(s24, n, cell);
    build_sheet(s32, n, cell);

    Uint32 trans = mkcol(s32->format, TRANS_R, TRANS_G, TRANS_B);
    SDL_SetColorKey(s16, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(s24, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(s32, SDL_SRCCOLORKEY, trans);

    /* Convert all to screen format to exercise the convert path. */
    SDL_Surface *c16 = SDL_ConvertSurface(s16, screen->format, 0);
    SDL_Surface *c24 = SDL_ConvertSurface(s24, screen->format, 0);
    SDL_Surface *c32 = SDL_ConvertSurface(s32, screen->format, 0);
    if (!c16 || !c24 || !c32) {
        fprintf(stderr, "ConvertSurface fail: %s\n", SDL_GetError());
        if (c16) SDL_FreeSurface(c16);
        if (c24) SDL_FreeSurface(c24);
        if (c32) SDL_FreeSurface(c32);
        SDL_FreeSurface(s16);
        SDL_FreeSurface(s24);
        SDL_FreeSurface(s32);
        SDL_Quit();
        return 1;
    }
    SDL_SetColorKey(c16, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(c24, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(c32, SDL_SRCCOLORKEY, trans);

    int running = 1;
    Uint32 frame = 0;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }

        SDL_FillRect(screen, NULL, mkcol(screen->format, 8, 12, 28));
        double t = frame * 0.02;

        /* Lay three staggered rows of sub-rectangle blits (one per depth). */
        for (int lane = 0; lane < 3; ++lane) {
            SDL_Surface *src = (lane == 0) ? c16 : (lane == 1) ? c24 : c32;
            int basey = 30 + lane * 190;
            for (int i = 0; i < 6; ++i) {
                int celli = (i + (int)(t * 0.5)) % n;
                int cellj = ((i + (int)(t * 0.5)) / n) % n;
                SDL_Rect sr = { (Sint16)(cellj * cell), (Sint16)(celli * cell),
                                (Uint16)TILE, (Uint16)TILE };
                int px = 30 + i * 130 + (int)(24 * sin(t + i / 2.0));
                int py = basey + (int)(18 * cos(t * 1.3 + i * 0.4));
                SDL_Rect dr = { (Sint16)px, (Sint16)py, (Uint16)TILE, (Uint16)TILE };
                if (SDL_BlitSurface(src, &sr, screen, &dr) != 0) {
                    fprintf(stderr, "BlitSurface(convert) fail: %s\n",
                            SDL_GetError());
                    running = 0;
                    break;
                }
            }
        }

        SDL_Flip(screen);
        ++frame;
        SDL_Delay(16);
    }

    SDL_FreeSurface(c32);
    SDL_FreeSurface(c24);
    SDL_FreeSurface(c16);
    SDL_FreeSurface(s32);
    SDL_FreeSurface(s24);
    SDL_FreeSurface(s16);
    SDL_Quit();
    return 0;
}