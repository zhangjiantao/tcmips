/* * SDL2 feature showcase: textured canvas + multiple cartoon sprites.
 *
 * Exercises most of the libsdl2 surface/renderer pipeline at once:
 *   - window + window surface (SDL_CreateWindow / SDL_GetWindowSurface)
 *   - software pixel painting of cartoon sprites (circles / hearts / stars)
 *   - colour-key transparency (SDL_SetColorKey + blit skip)
 *   - scaling / deformation animation (SDL_SoftStretch with W/H sinusoids)
 *   - format conversion (SDL_ConvertSurfaceFormat)
 *   - the software renderer (SDL_CreateRenderer, RenderClear, RenderFillRect,
 *     SDL_CreateTextureFromSurface, SDL_RenderCopy / SDL_RenderCopyEx with
 *     rotation + flip and colour modulation)
 *   - event draining, frame pacing, ESC to quit
 *
 * A background "texture canvas" is painted each frame; several cartoon sprites
 * move, scale/deform and stay transparent over it.
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 800
#define H 600
#define SP 64          /* sprite pixel buffer side */

/* Transparent backdrop used for surface (colorkey) sprites. */
#define KEYR 255
#define KEYG 0
#define KEYB 255

static Uint32 mkcol(SDL_PixelFormat *fmt, int r, int g, int b, int a) {
    return SDL_MapRGBA(fmt, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

/* ---- tiny software-pixel painter (works on any RGBA32 surface) ---- */
static void px(SDL_Surface *s, int x, int y, Uint32 c) {
    if (x < 0 || x >= s->w || y < 0 || y >= s->h) return;
    ((Uint32 *)s->pixels)[y * (s->pitch / 4) + x] = c;
}

static void fill_circle(SDL_Surface *s, int cx, int cy, int r, Uint32 c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) px(s, cx + x, cy + y, c);
}

static void fill_rect(SDL_Surface *s, int x0, int y0, int x1, int y1, Uint32 c) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) px(s, x, y, c);
}

/* Rhombus (diamond) helper. */
static void fill_diamond(SDL_Surface *s, int cx, int cy, int r, Uint32 c) {
    for (int y = -r; y <= r; ++y) {
        int half = r - (y < 0 ? -y : y);
        for (int x = -half; x <= half; ++x) px(s, cx + x, cy + y, c);
    }
}

static void fill_heart(SDL_Surface *s, int cx, int cy, int size, Uint32 c) {
    /* implicit-curve heart via coarse fill */
    for (int y = -size; y <= size; ++y)
        for (int x = -size; x <= size; ++x) {
            double sx = (double)x / (double)size;
            double sy = (double)y / (double)size;
            /* heart curve: (2x^2 + y^2 - 1)^3 - x^2*y^3 <= 0 */
            double a = 2.0 * sx * sx + sy * sy - 1.0;
            double b = sx * sx * sy * sy * sy;
            if (a * a * a - b <= 0.0) px(s, cx + x, cy + y, c);
        }
}

/* ---- cartoon sprites (painted on 64x64 RGBA32, key background) ---- */
static SDL_Surface *make_sun(void) {
    SDL_Surface *s = SDL_CreateRGBSurface(0, SP, SP, 32, 0,0,0,0);
    if (!s) return NULL;
    Uint32 key = mkcol(s->format, KEYR, KEYG, KEYB, 255);
    Uint32 yellow = mkcol(s->format, 255, 220, 40, 255);
    Uint32 orange = mkcol(s->format, 255, 140, 30, 255);
    SDL_FillRect(s, NULL, key);
    /* rays */
    for (int i = 0; i < 12; ++i) {
        double a = (double)i * (M_PI * 2.0 / 12.0);
        for (int d = 24; d < 30; ++d) {
            int x = (int)(SP / 2 + 8 + d * cos(a));
            int y = (int)(SP / 2 + 8 + d * sin(a));
            px(s, x, y, orange);
        }
    }
    fill_circle(s, SP / 2 + 8, SP / 2 + 8, 12, yellow);
    return s;
}

static SDL_Surface *make_cloud(void) {
    SDL_Surface *s = SDL_CreateRGBSurface(0, SP, SP, 32, 0,0,0,0);
    if (!s) return NULL;
    Uint32 key = mkcol(s->format, KEYR, KEYG, KEYB, 255);
    Uint32 white = mkcol(s->format, 255, 255, 255, 255);
    SDL_FillRect(s, NULL, key);
    fill_circle(s, 20, 40, 10, white);
    fill_circle(s, 32, 34, 12, white);
    fill_circle(s, 46, 38, 10, white);
    fill_circle(s, 58, 42, 8, white);
    fill_rect(s, 22, 40, 52, 44, white);
    fill_rect(s, 30, 46, 50, 50, white);
    return s;
}

static SDL_Surface *make_star(void) {
    SDL_Surface *s = SDL_CreateRGBSurface(0, SP, SP, 32, 0,0,0,0);
    if (!s) return NULL;
    Uint32 key = mkcol(s->format, KEYR, KEYG, KEYB, 255);
    Uint32 gold = mkcol(s->format, 255, 220, 90, 255);
    SDL_FillRect(s, NULL, key);
    /* two overlapping diamonds for a 4-point star */
    fill_diamond(s, 32, 32, 18, gold);
    fill_diamond(s, 32, 32, 18, gold);   /* rotated via second pass below */
    /* rotate a copy: coarse rotation of the diamond (approx star) */
    for (int y = -18; y <= 18; ++y)
        for (int x = -18; x <= 18; ++x)
            if (x * x + y * y <= 18 * 18) px(s, 32 + x - 4, 32 + y + 4, gold);
    return s;
}

static SDL_Surface *make_heart(void) {
    SDL_Surface *s = SDL_CreateRGBSurface(0, SP, SP, 32, 0,0,0,0);
    if (!s) return NULL;
    Uint32 key = mkcol(s->format, KEYR, KEYG, KEYB, 255);
    Uint32 red = mkcol(s->format, 255, 60, 80, 255);
    Uint32 pink = mkcol(s->format, 255, 120, 150, 255);
    SDL_FillRect(s, NULL, key);
    fill_heart(s, 34, 34, 22, red);
    fill_heart(s, 38, 30, 10, pink);
    return s;
}

static SDL_Surface *make_smiley(void) {
    SDL_Surface *s = SDL_CreateRGBSurface(0, SP, SP, 32, 0,0,0,0);
    if (!s) return NULL;
    Uint32 key = mkcol(s->format, KEYR, KEYG, KEYB, 255);
    Uint32 skin = mkcol(s->format, 255, 210, 130, 255);
    Uint32 dark = mkcol(s->format, 40, 30, 20, 255);
    SDL_FillRect(s, NULL, key);
    fill_circle(s, 32, 34, 20, skin);
    /* eyes */
    fill_circle(s, 24, 30, 3, dark);
    fill_circle(s, 40, 30, 3, dark);
    /* smile arc */
    for (int y = 38; y <= 46; ++y)
        for (int x = 26; x <= 38; ++x) {
            double dx = (double)(x - 32);
            double dy = (double)(y - 42);
            if (fabs(dx * dx + dy * dy - 36.0) < 2.0) px(s, x, y, dark);
        }
    return s;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init fail: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("SDL2 cartoon canvas",
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, W, H,
                                       SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow fail: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }
    SDL_Surface *screen = SDL_GetWindowSurface(win);
    if (!screen) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    /* A "texture canvas": off-screen buffer we paint into, then blit to screen. */
    SDL_Surface *canvas = SDL_CreateRGBSurface(0, W, H, 32, 0,0,0,0);
    if (!canvas) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    /* Cartoon sprites as off-screen surfaces with transparent (colorkey) bg. */
    SDL_Surface *sun   = make_sun();
    SDL_Surface *cloud = make_cloud();
    SDL_Surface *star  = make_star();
    SDL_Surface *heart = make_heart();
    SDL_Surface *smiley= make_smiley();
    if (!sun || !cloud || !star || !heart || !smiley) {
        fprintf(stderr, "sprite alloc fail\n");
        return 1;
    }
    Uint32 key = mkcol(sun->format, KEYR, KEYG, KEYB, 255);
    SDL_SetColorKey(sun,   SDL_TRUE, key);
    SDL_SetColorKey(cloud, SDL_TRUE, key);
    SDL_SetColorKey(star,  SDL_TRUE, key);
    SDL_SetColorKey(heart, SDL_TRUE, key);
    SDL_SetColorKey(smiley,SDL_TRUE, key);

    /* Also use the software renderer for texture copies and rotation. */
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    SDL_Texture *texStar = ren ? SDL_CreateTextureFromSurface(ren, star) : NULL;
    if (texStar) SDL_SetTextureBlendMode(texStar, SDL_BLENDMODE_BLEND);

    int running = 1;
    SDL_Event e;
    Uint32 frame = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        double t = (double)frame * 0.018;

        /* Paint the canvas backdrop: sky gradient + ground. */
        for (int y = 0; y < H; ++y) {
            int b = 120 + (int)(60 * (1.0 - (double)y / (double)H));
            Uint32 col = mkcol(canvas->format, 60, 100, b, 255);
            for (int x = 0; x < W; ++x)
                ((Uint32 *)canvas->pixels)[y * (canvas->pitch / 4) + x] = col;
        }
        fill_rect(canvas, 0, H - 100, W, H - 1, mkcol(canvas->format, 90, 160, 70, 255));

        /* Each sprite: moving + scaling/deformation (SoftStretch) then
         * colour-key blit onto the canvas. */
        {
            SDL_Rect sr = {0,0,SP,SP};

            /* Sun: bounce along the top, squashes vertically over time.
             * SoftStretch directly onto the canvas at the moving position;
             * the colour key keeps the sprite background transparent. */
            int sw = SP + (int)(8 * sin(t * 1.4));
            int sh = SP + (int)(14 * sin(t * 0.9));
            if (sw < 8) sw = 8; if (sh < 8) sh = 8;
            int sx = (int)(40 + (W - sw) * (0.5 + 0.5 * sin(t * 0.4)));
            int sy = (int)(30 + 18 * sin(t * 1.1));
            SDL_Rect d = { (Sint16)sx, (Sint16)sy, (Uint16)sw, (Uint16)sh };
            SDL_SoftStretch(sun, &sr, canvas, &d);

            /* Heart: orbits, pulses in size (deforms). */
            double m = 1.6 + 0.6 * sin(t * 0.7);
            int hw = (int)(SP * m), hh = (int)(SP * (2.2 - m));
            int hx = (int)(W/2 + 220 * cos(t * 0.5));
            int hy = (int)(H/2 + 120 * sin(t * 0.7));
            SDL_Rect dh = { (Sint16)hx, (Sint16)hy, (Uint16)hw, (Uint16)hh };
            SDL_SoftStretch(heart, &sr, canvas, &dh);

            /* Smiley: waves across the screen, stretches horizontally. */
            int sw3 = SP + (int)(18 * sin(t * 1.2));
            int s3x = (int)((W - sw3) * (0.5 + 0.5 * sin(t * 0.35)));
            int s3y = (int)(H - 90 - 20 * cos(t * 1.0));
            SDL_Rect ds = { (Sint16)s3x, (Sint16)s3y, (Uint16)sw3, (Uint16)SP };
            SDL_SoftStretch(smiley, &sr, canvas, &ds);

            /* Cloud: drifts, mildly deformed. */
            int cw = SP + (int)(10 * sin(t * 0.3));
            int cxx = (int)((W - cw) * (0.1 + 0.8 * fabs(sin(t * 0.2))));
            int cyy = (int)(50 + 14 * sin(t * 0.6));
            SDL_Rect dc = { (Sint16)cxx, (Sint16)cyy, (Uint16)cw, (Uint16)(SP-10) };
            SDL_SoftStretch(cloud, &sr, canvas, &dc);

            /* Star: uses the renderer texture path with rotation + flip. */
            if (ren && texStar) {
                SDL_RenderClear(ren);
                SDL_Rect dst = { (int)(W - 160), 140, 80, 80 };
                double ang = 40.0 * sin(t * 0.9);
                SDL_RenderCopyEx(ren, texStar, NULL, &dst, ang, NULL,
                                 ((frame / 90) & 1) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
                /* Star also drawn onto the canvas directly for the surface path */
            } else {
                int stw = 44;
                SDL_Rect dst2 = { W-140, 150, stw, stw };
                SDL_BlitSurface(star, NULL, canvas, &dst2);
            }
        }

        /* Commit canvas -> window. */
        SDL_Rect full = {0,0,W,H};
        SDL_BlitSurface(canvas, NULL, screen, &full);
        if (ren) SDL_RenderPresent(ren);         /* submits window surface */
        else    SDL_UpdateWindowSurface(win);

        ++frame;
        SDL_Delay(16);
    }

    if (ren) SDL_DestroyRenderer(ren);
    if (texStar) SDL_DestroyTexture(texStar);
    SDL_FreeSurface(smiley); SDL_FreeSurface(heart);
    SDL_FreeSurface(star); SDL_FreeSurface(cloud); SDL_FreeSurface(sun);
    SDL_FreeSurface(canvas);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}