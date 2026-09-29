/* * SDL1 test: SDL_SoftStretch scaling and deformation.
 *
 * Exercises the libsdl scale / deform path:
 *   - SDL_SoftStretch          (nearest-neighbour rescale between surfaces)
 *   - SDL_CreateRGBSurface     (source and destination)
 *   - SDL_BlitSurface          (blit scaled result to the screen)
 *   - SDL_SetColorKey          (keep the stretched sprite transparent)
 *
 * The animation drives the destination rectangle's width and height with
 * independent sinusoids so the source square is both scaled and deformed
 * (aspect ratio changes) every frame, along with a horizontal/vertical
 * translation.  Press ESC to quit.
 */

#include <SDL/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600
#define SPR 64          /* source sprite size */

#define TRANS_R 255
#define TRANS_G 0
#define TRANS_B 255

static Uint32 mkcol(SDL_PixelFormat *fmt, int r, int g, int b)
{
    return SDL_MapRGB(fmt, (Uint8)r, (Uint8)g, (Uint8)b);
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
    SDL_WM_SetCaption("SDL1 SoftStretch scale / deform test", NULL);

    /* Source sprite: 64x64 32-bit ARGB, checked pattern on magenta. */
    SDL_Surface *src = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 32,
                                            0x00FF0000, 0x0000FF00,
                                            0x000000FF, 0xFF000000);
    if (!src) {
        fprintf(stderr, "CreateRGBSurface fail: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    Uint32 trans = mkcol(src->format, TRANS_R, TRANS_G, TRANS_B);
    SDL_FillRect(src, NULL, trans);
    Uint32 c1 = mkcol(src->format, 60, 220, 120);
    Uint32 c2 = mkcol(src->format, 255, 160, 60);
    {
        for (int y = 0; y < SPR; ++y) {
            unsigned char *row = (unsigned char *)src->pixels + (size_t)y * src->pitch;
            for (int x = 0; x < SPR; ++x) {
                Uint32 c = ((x ^ y) / 8) & 1 ? c1 : c2;
                *(Uint32 *)(row + (size_t)x * 4) = c;
            }
        }
        /* carve out a diamond that stays transparent */
        int cx = SPR / 2, cy = SPR / 2, r = 20;
        for (int y = -r; y <= r; ++y) {
            int halfw = r - (y < 0 ? -y : y);
            for (int x = -halfw; x <= halfw; ++x) {
                unsigned char *p = (unsigned char *)src->pixels +
                                   (size_t)(cy + y) * src->pitch +
                                   (size_t)(cx + x) * 4;
                *(Uint32 *)p = trans;
            }
        }
    }
    SDL_SetColorKey(src, SDL_SRCCOLORKEY, trans);

    /* Scaled destination surface, fixed at a generous max size. */
    SDL_Surface *dst = SDL_CreateRGBSurface(SDL_SWSURFACE, 400, 300, 32,
                                            screen->format->Rmask,
                                            screen->format->Gmask,
                                            screen->format->Bmask,
                                            screen->format->Amask);
    if (!dst) {
        fprintf(stderr, "CreateRGBSurface(dst) fail: %s\n", SDL_GetError());
        SDL_FreeSurface(src);
        SDL_Quit();
        return 1;
    }
    SDL_SetColorKey(dst, SDL_SRCCOLORKEY, trans);

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

        SDL_FillRect(screen, NULL, mkcol(screen->format, 6, 10, 24));

        double t = frame * 0.02;

        /* Independent scale factors produce both scaling and deformation. */
        int sw = SPR + (int)(96 * sin(t * 0.8));
        int sh = SPR + (int)(64 * cos(t * 0.55));
        if (sw < 8) sw = 8;
        if (sh < 8) sh = 8;

        SDL_Rect srect = { 0, 0, (Uint16)SPR, (Uint16)SPR };
        SDL_Rect drect = { 0, 0, (Uint16)sw, (Uint16)sh };

        /* Strech into the working surface. */
        if (SDL_SoftStretch(src, &srect, dst, &drect) != 0) {
            fprintf(stderr, "SoftStretch fail: %s\n", SDL_GetError());
            break;
        }

        int sx = (int)(W / 2 - sw / 2 + 180 * cos(t * 0.6));
        int sy = (int)(H / 2 - sh / 2 + 120 * sin(t * 0.4));
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;

        SDL_Rect bl = { (Sint16)sx, (Sint16)sy, (Uint16)sw, (Uint16)sh };
        if (SDL_BlitSurface(dst, &drect, screen, &bl) != 0) {
            fprintf(stderr, "BlitSurface(stretched) fail: %s\n", SDL_GetError());
            break;
        }

        SDL_Flip(screen);
        ++frame;
        SDL_Delay(16);
    }

    SDL_FreeSurface(dst);
    SDL_FreeSurface(src);
    SDL_Quit();
    return 0;
}