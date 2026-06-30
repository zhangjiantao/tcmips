//
// Created by Admin on 2026/6/14.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BEZIER_STEPS 24

#define BANDS_COUNT 16
#define MAX_LINES_PER_BAND 16
#define TOTAL_MAX_LINES (BANDS_COUNT * MAX_LINES_PER_BAND)

#define BAND_OFFSET 16

typedef unsigned int pixel_t;

#define RGB888(r, g, b)                                                        \
  (((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (r))
#define ABS(x) ((x) < 0 ? -(x) : (x))

typedef struct {
  int x, y;
  int vx, vy;
} MovePoint;

typedef struct {
  int x0, y0, x1, y1, x2, y2, x3, y3;
} LineTrack;

LineTrack track_history[TOTAL_MAX_LINES];
int history_head = 0;
int history_count = 0;

void init_point(MovePoint *p, int x, int y, int vx, int vy) {
  p->x = x;
  p->y = y;
  p->vx = vx;
  p->vy = vy;
}

void update_point(MovePoint *p) {
  p->x += p->vx;
  p->y += p->vy;
  if (p->x <= 0 || p->x >= SCREEN_WIDTH) {
    p->vx = -p->vx;
    p->x += p->vx;
  }
  if (p->y <= 0 || p->y >= SCREEN_HEIGHT) {
    p->vy = -p->vy;
    p->y += p->vy;
  }
}

inline void draw_pixel(pixel_t *buffer, int x, int y, pixel_t color) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    buffer[y * SCREEN_WIDTH + x] = color;
  }
}

void draw_line(pixel_t *buffer, int x0, int y0, int x1, int y1, pixel_t color) {
  int dx = ABS(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -ABS(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  while (1) {
    draw_pixel(buffer, x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err << 1;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void draw_bezier_curve(pixel_t *buffer, int x0, int y0, int x1, int y1, int x2,
                       int y2, int x3, int y3, pixel_t color) {
  int last_x = x0, last_y = y0;
  int step = 128 / BEZIER_STEPS;
  for (int t = step; t <= 128; t += step) {
    int mt = 128 - t;
    int mt3 = mt * mt * mt;
    int t3 = t * t * t;
    int mt2_t = (mt * mt * t) * 3;
    int mt_t2 = (mt * t * t) * 3;
    int node_x = (mt3 * x0 + mt2_t * x1 + mt_t2 * x2 + t3 * x3) >> 21;
    int node_y = (mt3 * y0 + mt2_t * y1 + mt_t2 * y2 + t3 * y3) >> 21;
    draw_line(buffer, last_x, last_y, node_x, node_y, color);
    last_x = node_x;
    last_y = node_y;
  }
}

void write_to_hardware_fb(const pixel_t *render_buf, size_t size);

static pixel_t render_buffer[SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(pixel_t)];
static size_t buffer_size = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(pixel_t);

void screensaver(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_WIDTH);
  tcm_pixel_console_clear();

  memset(render_buffer, 0, buffer_size);

  MovePoint p0, p1, p2, p3;
  init_point(&p0, SCREEN_WIDTH / 4, SCREEN_HEIGHT / 4, 3, 2);
  init_point(&p1, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3, -2, 4);
  init_point(&p2, SCREEN_WIDTH / 3, SCREEN_HEIGHT / 2, 4, -1);
  init_point(&p3, SCREEN_WIDTH * 3 / 4, SCREEN_HEIGHT * 3 / 4, -3, -3);

  unsigned char r = 255, g = 0, b = 0;
  int color_state = 0;

  while (1) {
    if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
      return;

    if (history_count >= TOTAL_MAX_LINES) {
      for (int b_idx = 0; b_idx < BANDS_COUNT; b_idx++) {
        int tail =
            (history_head + TOTAL_MAX_LINES - history_count) % TOTAL_MAX_LINES;
        LineTrack old = track_history[tail];
        draw_bezier_curve(render_buffer, old.x0, old.y0, old.x1, old.y1, old.x2,
                          old.y2, old.x3, old.y3, 0);
        history_count--;
      }
    }

    update_point(&p0);
    update_point(&p1);
    update_point(&p2);
    update_point(&p3);

    if (color_state == 0) {
      g += 8;
      if (g >= 248)
        color_state = 1;
    } else if (color_state == 1) {
      r -= 8;
      if (r <= 8)
        color_state = 2;
    } else if (color_state == 2) {
      b += 8;
      if (b >= 248)
        color_state = 3;
    } else if (color_state == 3) {
      g -= 8;
      if (g <= 8)
        color_state = 4;
    } else if (color_state == 4) {
      r += 8;
      if (r >= 248)
        color_state = 5;
    } else if (color_state == 5) {
      b -= 8;
      if (b <= 8)
        color_state = 0;
    }

    for (int b_idx = 0; b_idx < BANDS_COUNT; b_idx++) {
      int offset = b_idx * BAND_OFFSET;
      int cur_x0 = p0.x + offset, cur_y0 = p0.y + offset;
      int cur_x1 = p1.x + offset, cur_y1 = p1.y - offset;
      int cur_x2 = p2.x - offset, cur_y2 = p2.y + offset;
      int cur_x3 = p3.x - offset, cur_y3 = p3.y - offset;

      unsigned char cur_r = r, cur_g = g, cur_b = b;
      if (b_idx == 1) {
        cur_r = g;
        cur_g = b;
        cur_b = r;
      }
      if (b_idx == 2) {
        cur_r = b;
        cur_g = r;
        cur_b = g;
      }
      pixel_t current_color = RGB888(cur_r, cur_g, cur_b);

      draw_bezier_curve(render_buffer, cur_x0, cur_y0, cur_x1, cur_y1, cur_x2,
                        cur_y2, cur_x3, cur_y3, current_color);

      track_history[history_head].x0 = cur_x0;
      track_history[history_head].y0 = cur_y0;
      track_history[history_head].x1 = cur_x1;
      track_history[history_head].y1 = cur_y1;
      track_history[history_head].x2 = cur_x2;
      track_history[history_head].y2 = cur_y2;
      track_history[history_head].x3 = cur_x3;
      track_history[history_head].y3 = cur_y3;

      history_head = (history_head + 1) % TOTAL_MAX_LINES;
      history_count++;
    }

    write_to_hardware_fb(render_buffer, buffer_size);
  }
}

void write_to_hardware_fb(const pixel_t *render_buf, size_t size) {
  memcpy((void *)TCM_VRAM_PIXEL_ADDR, render_buf, size);
}
