#include <SDL/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600

static void put_pixel(SDL_Surface *surf, int px, int py, Uint8 idx)
{
    if (px < 0 || px >= W || py < 0 || py >= H)
        return;
    Uint8 *p = (Uint8 *)surf->pixels + py * surf->pitch + px;
    *p = idx;
}

static void draw_circle(SDL_Surface *surf, int cx, int cy, int r, Uint8 idx)
{
    for (int y = -r; y <= r; y++)
    {
        for (int x = -r; x <= r; x++)
        {
            if (x*x + y*y <= r*r)
            {
                put_pixel(surf, cx + x, cy + y, idx);
            }
        }
    }
}

static void draw_line(SDL_Surface *surf, int x0, int y0, int x1, int y1, Uint8 idx)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;

    for (;;)
    {
        put_pixel(surf, x, y, idx);
        if (x == x1 && y == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init fail: %s\n", SDL_GetError());
        return 1;
    }

    /* 8bit 调色板模式 */
    SDL_Surface *screen = SDL_SetVideoMode(W, H, 8, SDL_SWSURFACE | SDL_HWPALETTE);
    if (!screen)
    {
        fprintf(stderr, "SDL_SetVideoMode fail: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_WM_SetCaption("SDL1.2 8‑bit Palette Animation", NULL);

    /* 构建256色调色板 */
    SDL_Color pal[256];
    for (int i = 0; i < 256; i++)
    {
        pal[i].r = 0;
        pal[i].g = 0;
        pal[i].b = 0;
        pal[i].unused = 0;
    }
    /* 索引0：背景深蓝 */
    pal[0].r = 10; pal[0].g = 15; pal[0].b = 30;

    /* 1~127：彩色色系给线条、圆 */
    for (int i = 1; i <= 127; i++)
    {
        pal[i].r = (i * 7) & 0xff;
        pal[i].g = (128 + i * 5) & 0xff;
        pal[i].b = (200 + i * 3) & 0xff;
    }
    /* 128：方块黄色 */
    pal[128].r = 255; pal[128].g = 220; pal[128].b = 80;

    SDL_SetPalette(screen, SDL_LOGPAL | SDL_PHYSPAL, pal, 0, 256);

    int running = 1;
    Uint32 frame = 0;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }

        /* 清屏：全部填充背景索引0 */
        SDL_FillRect(screen, NULL, 0);

        double t = frame * 0.02;
        int cx = W / 2;
        int cy = H / 2;

        /* 旋转放射线条，复用1‑127调色索引 */
        for (int i = 0; i < 36; i++)
        {
            double ang = t + i * (M_PI * 2 / 36.0);
            int x2 = cx + (int)(220 * cos(ang));
            int y2 = cy + (int)(220 * sin(ang));
            Uint8 idx = 1 + (i % 127);
            draw_line(screen, cx, cy, x2, y2, idx);
        }

        /* 漂移同心圆 */
        for (int r = 30; r <= 180; r += 30)
        {
            int offX = (int)(60 * cos(t * 1.3 + r * 0.05));
            int offY = (int)(60 * sin(t * 0.9 + r * 0.05));
            Uint8 idx = 1 + ((r / 2) % 127);
            draw_circle(screen, cx + offX, cy + offY, r / 4, idx);
        }

        /* 波动方块阵列，使用索引128黄色 */
        for (int j = 0; j < 6; j++)
        {
            for (int i = 0; i < 8; i++)
            {
                int sx = 40 + i * 90 + (int)(12 * sin(t * 2.0 + i + j));
                int sy = 40 + j * 90 + (int)(12 * cos(t * 2.0 + i + j));
                SDL_Rect rc = {sx, sy, 16, 16};
                SDL_FillRect(screen, &rc, 128);
            }
        }

        /* 调色板滚动特效：只修改调色板，不需要重绘画面也能变色 */
        int shift = (frame / 4) % 127;
        SDL_Color tmp[127];
        for(int k=0;k<127;k++)
            tmp[k] = pal[1 + ((k + shift) % 127)];
        SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, tmp, 1, 127);

        SDL_Flip(screen);
        frame++;
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
