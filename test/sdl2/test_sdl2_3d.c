/* * SDL2: simplest rotating cube, six faces, six colours, no lighting.
 *
 * Renders a rotating cube with libsdl2 surface APIs.  Each of the six faces
 * uses one fixed colour, faces are filled as quads with a scanline fill,
 * sorted back-to-front (painter's algorithm), then presented via
 * SDL_UpdateWindowSurface.  No lighting or texturing.
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600
#define NVERTS 8
#define NFACES 6

static const float g_verts[NVERTS][3] = {
    {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
    {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1},
};
static const int g_faces[NFACES][4] = {
    {0,1,2,3},{4,5,6,7},{0,4,7,3},{1,5,6,2},{3,2,6,7},{0,1,5,4},
};

/* One RGB colour per face. */
static const int g_face_col[NFACES][3] = {
    {220, 60, 60},   /* red    */
    {60, 220, 60},   /* green  */
    {60, 60, 220},   /* blue   */
    {240, 200, 40},  /* yellow */
    {240, 120, 40},  /* orange */
    {180, 60, 220},  /* purple */
};

static void setpx(SDL_Surface *s, int x, int y, Uint32 c) {
    if (x < 0 || x >= s->w || y < 0 || y >= s->h) return;
    ((Uint32 *)(s->pixels))[y * (s->pitch >> 2) + x] = c;
}

/* Scanline-fill a convex quad (even-odd x-span). */
static void fill_quad(SDL_Surface *s, const float pts[][2], Uint32 c) {
    float ymin = pts[0][1], ymax = pts[0][1];
    for (int i = 1; i < 4; ++i) {
        if (pts[i][1] < ymin) ymin = pts[i][1];
        if (pts[i][1] > ymax) ymax = pts[i][1];
    }
    if (ymax < 0 || ymin >= s->h) return;
    int y0 = (int)floorf(ymin), y1 = (int)ceilf(ymax);
    if (y0 < 0) y0 = 0;
    if (y1 >= s->h) y1 = s->h - 1;
    for (int y = y0; y <= y1; ++y) {
        double lx = s->w + 1.0, rx = -1.0;
        for (int i = 0, j = 3; i < 4; j = i++) {
            float yi = pts[i][1], yj = pts[j][1];
            if ((yi <= (float)y && yj > (float)y) || (yj <= (float)y && yi > (float)y)) {
                double t0 = ((double)y - yi) / (double)(yj - yi);
                double ex = pts[i][0] + t0 * (pts[j][0] - pts[i][0]);
                if (ex < lx) lx = ex;
                if (ex > rx) rx = ex;
            }
        }
        int a = (int)ceil(lx), b = (int)floor(rx);
        if (a < 0) a = 0;
        if (b >= s->w) b = s->w - 1;
        for (int x = a; x <= b; ++x) setpx(s, x, y, c);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init fail\n"); return 1; }
    SDL_Window *win = SDL_CreateWindow("SDL2 rotating cube", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_SHOWN);
    if (!win) { SDL_Quit(); return 1; }
    SDL_Surface *screen = SDL_GetWindowSurface(win);
    if (!screen) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    double t = 0.0;
    while (1) {
        SDL_Event e; int quit = 0;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = 1;
        }
        if (quit) break;

        float rx=(float)(t*0.7), ry=(float)(t*1.05);
        float cxx=cosf(rx), sxx=sinf(rx), cyy=cosf(ry), syy=sinf(ry);
        float scale=130.0f, cx=W/2.0f, cy=H/2.0f;

        float wv[NVERTS][3];
        for (int i=0;i<NVERTS;++i) {
            float x0=g_verts[i][0], y0=g_verts[i][1], z0=g_verts[i][2];
            float x1=cyy*x0+syy*z0, z1=-syy*x0+cyy*z0;
            wv[i][0]=x1; wv[i][1]=cxx*y0-sxx*z1; wv[i][2]=sxx*y0+cxx*z1;
        }

        float quad[NFACES][4][2];
        float depth[NFACES];
        int order[NFACES]={0,1,2,3,4,5};
        for (int f=0;f<NFACES;++f) {
            float cz=0;
            for (int k=0;k<4;++k) {
                int vi=g_faces[f][k];
                quad[f][k][0]=cx + wv[vi][0]*scale;
                quad[f][k][1]=cy - wv[vi][1]*scale;
                cz+=wv[vi][2];
            }
            depth[f]=cz/4.0f;
        }
        /* painter sort: draw far faces first (largest view-z first in list). */
        for (int i=0;i<NFACES-1;++i) for (int j=i+1;j<NFACES;++j)
            if (depth[order[i]]>depth[order[j]]) { int t2=order[i];order[i]=order[j];order[j]=t2; }

        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 8, 10, 26));

        for (int i=0;i<NFACES;++i) {
            int f=order[i];
            Uint32 c=SDL_MapRGB(screen->format, g_face_col[f][0], g_face_col[f][1], g_face_col[f][2]);
            fill_quad(screen, quad[f], c);
        }

        SDL_UpdateWindowSurface(win);
        t += 0.016;
        SDL_Delay(16);
    }

    SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}