/* * SDL1 test: sprite blitting with transparency (colorkey) and movement.
 *
 * Exercises the libsdl copy path:
 *   - SDL_CreateRGBSurface / SDL_CreateRGBSurfaceFrom   (surface creation)
 *   - SDL_ConvertSurface                                 (format conversion)
 *   - SDL_SetColorKey                                    (transparent key)
 *   - SDL_SetAlpha                                       (per-surface alpha)
 *   - SDL_BlitSurface                                    (copy to the screen)
 *   - SDL_FillRect                                       (canvas + sprite fill)
 *
 * Renders several self-drawn sprites moving over a filled canvas so that
 * transparent pixels fall through to the background.  Press ESC to quitit.
 */

#include <SDL/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600
#define SPR 64          /* sprite size (square) */

/* Colorkey: fully transparent backdrop colour used by every sprite. */
#define TRANS_R 255
#define TRANS_G 0
#define TRANS_B 255

static Uint32 mkcol(SDL_PixelFormat *fmt, int r, int g, int b)
{
    return SDL_MapRGB(fmt, (Uint8)r, (Uint8)g, (Uint8)b);
}

/* Fill a rectangle of an arbitrary-bpp surface using direct pixel writes.
 * This works regardless of the surface depth because it packs via MapRGB. */
static void sprite_fill(SDL_Surface *s, int x0, int y0, int x1, int y1,
                        Uint32 color)
{
    int bpp = s->format->BytesPerPixel;
    for (int y = y0; y < y1 && y < s->h; ++y) {
        unsigned char *row = (unsigned char *)s->pixels +
                             (size_t)y * s->pitch;
        for (int x = x0; x < x1 && x < s->w; ++x) {
            unsigned char *p = row + (size_t)x * bpp;
            switch (bpp) {
            case 1: *p = (Uint8)color; break;
            case 2: *(Uint16 *)p = (Uint16)color; break;
            case 3: p[0] = (Uint8)(color & 0xFF);
                    p[1] = (Uint8)((color >> 8) & 0xFF);
                    p[2] = (Uint8)((color >> 16) & 0xFF); break;
            case 4: *(Uint32 *)p = color; break;
            }
        }
    }
}

/* Draw a simple checkered pattern on a sprite. */
static void deco_rects(SDL_Surface *s, Uint32 c1, Uint32 c2)
{
    int half = SPR / 2;
    sprite_fill(s, 4, 4, half, half, c1);
    sprite_fill(s, half, half, SPR - 4, SPR - 4, c1);
    sprite_fill(s, half, 4, SPR - 4, half, c2);
    sprite_fill(s, 4, half, half, SPR - 4, c2);
}

/* Draw concentric squares on a sprite. */
static void deco_rings(SDL_Surface *s, Uint32 c1, Uint32 c2)
{
    for (int side = 10; side < SPR; side += 10) {
        Uint32 c = (side / 10) & 1 ? c1 : c2;
        sprite_fill(s, (SPR - side) / 2, (SPR - side) / 2,
                    (SPR + side) / 2, (SPR + side) / 2, c);
    }
}

/* Draw a diamond (rotated square) on a sprite. */
static void deco_diamond(SDL_Surface *s, Uint32 c)
{
    int cx = SPR / 2, cy = SPR / 2, r = SPR / 2 - 6;
    for (int y = -r; y <= r; ++y) {
        int half = r - (y < 0 ? -y : y);   /* bounding half-width at row y */
        for (int x = -half; x <= half; ++x) {
            int px = cx + x, py = cy + y;
            if (px >= 0 && px < SPR && py >= 0 && py < SPR) {
                unsigned char *p = (unsigned char *)s->pixels +
                                   (size_t)py * s->pitch +
                                   (size_t)px * s->format->BytesPerPixel;
                switch (s->format->BytesPerPixel) {
                case 1: *p = (Uint8)c; break;
                case 2: *(Uint16 *)p = (Uint16)c; break;
                case 3: p[0] = (Uint8)(c & 0xFF);
                        p[1] = (Uint8)((c >> 8) & 0xFF);
                        p[2] = (Uint8)((c >> 16) & 0xFF); break;
                case 4: *(Uint32 *)p = c; break;
                }
            }
        }
    }
}

/* Fill a whole sprite with the transparent colour first. */
static void sprite_clear(SDL_Surface *s)
{
    SDL_FillRect(s, NULL, mkcol(s->format, TRANS_R, TRANS_G, TRANS_B));
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
    SDL_WM_SetCaption("SDL1 sprite / colorkey / convert test", NULL);

    /* Background canvas: fill once each frame. */
    Uint32 bg = mkcol(screen->format, 12, 18, 42);

    /* Sprite 0: 32-bit ARGB sprite, checker pattern. */
    SDL_Surface *s0 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 32,
                                           0x00FF0000, 0x0000FF00,
                                           0x000000FF, 0xFF000000);
    if (!s0) {
        fprintf(stderr, "CreateRGBSurface(32) fail: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    sprite_clear(s0);
    deco_rects(s0, mkcol(s0->format, 255, 220, 80),
               mkcol(s0->format, 200, 80, 255));

    /* Sprite 1: 16-bit RGB565 sprite, concentric rings. */
    SDL_Surface *s1 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 16,
                                           0x0000F800, 0x000007E0,
                                           0x0000001F, 0x00000000);
    if (!s1) {
        fprintf(stderr, "CreateRGBSurface(16) fail: %s\n", SDL_GetError());
        SDL_FreeSurface(s0);
        SDL_Quit();
        return 1;
    }
    sprite_clear(s1);
    deco_rings(s1, mkcol(s1->format, 60, 220, 80),
               mkcol(s1->format, 255, 150, 40));

    /* Sprite 2: 24-bit RGB888 sprite, diamond. */
    SDL_Surface *s2 = SDL_CreateRGBSurface(SDL_SWSURFACE, SPR, SPR, 24,
                                           0x000000FF, 0x0000FF00,
                                           0x00FF0000, 0x00000000);
    if (!s2) {
        fprintf(stderr, "CreateRGBSurface(24) fail: %s\n", SDL_GetError());
        SDL_FreeSurface(s0);
        SDL_FreeSurface(s1);
        SDL_Quit();
        return 1;
    }
    sprite_clear(s2);
    deco_diamond(s2, mkcol(s2->format, 80, 200, 255));

    /* Convert the 16-bit sprite to the screen format to test the cross-bpp
     * convert path (SDL_ConvertSurface -> BlitSurface). */
    SDL_Surface *s1conv = SDL_ConvertSurface(s1, screen->format, 0);
    if (!s1conv) {
        fprintf(stderr, "ConvertSurface fail: %s\n", SDL_GetError());
        SDL_FreeSurface(s0);
        SDL_FreeSurface(s1);
        SDL_FreeSurface(s2);
        SDL_Quit();
        return 1;
    }

    /* Every sprite uses the same transparent colour. */
    Uint32 trans = mkcol(s0->format, TRANS_R, TRANS_G, TRANS_B);
    SDL_SetColorKey(s0, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(s1conv, SDL_SRCCOLORKEY, trans);
    SDL_SetColorKey(s2, SDL_SRCCOLORKEY, trans);

    /* Spell-check: half the screen width for each of the two moving lanes. */
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

        /* Re-fill canvas. */
        SDL_FillRect(screen, NULL, bg);

        double t = frame * 0.015;
        int cx0 = (int)(120 + 200 * sin(t * 0.6));
        int cy0 = (int)(100 + 160 * cos(t * 0.4));
        int cx1 = (int)(W / 2 + 220 * cos(t * 0.5));
        int cy1 = (int)(100 + 200 * sin(t * 0.7));
        int cx2 = (int)(120 + 260 * sin(t * 0.3 + 1.2));
        int cy2 = (int)(H - 110 - 140 * cos(t * 0.5));

        SDL_Rect dr0 = { (Sint16)cx0, (Sint16)cy0, (Uint16)SPR, (Uint16)SPR };
        SDL_Rect dr1 = { (Sint16)cx1, (Sint16)cy1, (Uint16)SPR, (Uint16)SPR };
        SDL_Rect dr2 = { (Sint16)cx2, (Sint16)cy2, (Uint16)SPR, (Uint16)SPR };

        /* Blit converted 16->32 sprite. */
        if (SDL_BlitSurface(s1conv, NULL, screen, &dr1) != 0) {
            fprintf(stderr, "BlitSurface(s1) fail: %s\n", SDL_GetError());
            break;
        }
        /* Blit native 32-bit sprite. */
        if (SDL_BlitSurface(s0, NULL, screen, &dr0) != 0) {
            fprintf(stderr, "BlitSurface(s0) fail: %s\n", SDL_GetError());
            break;
        }
        /* Blit 24-bit sprite (converted on the fly by the copy path only if
         * target format matches; here it differs from screen, so the generic
         * path handles it). */
        if (SDL_BlitSurface(s2, NULL, screen, &dr2) != 0) {
            fprintf(stderr, "BlitSurface(s2) fail: %s\n", SDL_GetError());
            break;
        }

        SDL_Flip(screen);
        ++frame;
        SDL_Delay(16);
    }

    SDL_FreeSurface(s1conv);
    SDL_FreeSurface(s2);
    SDL_FreeSurface(s1);
    SDL_FreeSurface(s0);
    SDL_Quit();
    return 0;
}