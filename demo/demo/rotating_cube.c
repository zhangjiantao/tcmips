//
// Created by zhangjiantao on 2026/5/18.
//

#include "stdlib.h"
#include "time.h"

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT (SCREEN_WIDTH / 4 * 3)

#define BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
static uint8_t MyFrameBuffer[BUFFER_SIZE];

#define TEXTURE_SIZE 32

#include "rotating_cube_tex.inc"

typedef struct {
  float x, y, z;
} Vector3;

typedef struct {
  int16_t x, y;
  float u;
  float v;
  float w;
} Vertex2D;

static const Vector3 CUBE_VERTICES[8] = {
    {-20.0f, -20.0f, -20.0f}, {20.0f, -20.0f, -20.0f}, {20.0f, 20.0f, -20.0f},
    {-20.0f, 20.0f, -20.0f},  {-20.0f, -20.0f, 20.0f}, {20.0f, -20.0f, 20.0f},
    {20.0f, 20.0f, 20.0f},    {-20.0f, 20.0f, 20.0f}};

static const uint8_t CUBE_FACES[6][4] = {{0, 1, 2, 3}, {1, 5, 6, 2},
                                         {5, 4, 7, 6}, {4, 0, 3, 7},
                                         {4, 5, 1, 0}, {3, 2, 6, 7}};

static const float FACE_UV[4][2] = {{0.0f, 0.0f},
                                    {TEXTURE_SIZE - 1, 0.0f},
                                    {TEXTURE_SIZE - 1, TEXTURE_SIZE - 1},
                                    {0.0f, TEXTURE_SIZE - 1}};

void drawTexturedTriangle(Vertex2D p0, Vertex2D p1, Vertex2D p2,
                          const uint8_t *texture) {

  float p0_u_w = p0.u * p0.w, p0_v_w = p0.v * p0.w;
  float p1_u_w = p1.u * p1.w, p1_v_w = p1.v * p1.w;
  float p2_u_w = p2.u * p2.w, p2_v_w = p2.v * p2.w;

  if (p0.y > p1.y) {
    Vertex2D t = p0;
    p0 = p1;
    p1 = t;
    float tf;
    tf = p0_u_w;
    p0_u_w = p1_u_w;
    p1_u_w = tf;
    tf = p0_v_w;
    p0_v_w = p1_v_w;
    p1_v_w = tf;
  }
  if (p0.y > p2.y) {
    Vertex2D t = p0;
    p0 = p2;
    p2 = t;
    float tf;
    tf = p0_u_w;
    p0_u_w = p2_u_w;
    p2_u_w = tf;
    tf = p0_v_w;
    p0_v_w = p2_v_w;
    p2_v_w = tf;
  }
  if (p1.y > p2.y) {
    Vertex2D t = p1;
    p1 = p2;
    p2 = t;
    float tf;
    tf = p1_u_w;
    p1_u_w = p2_u_w;
    p2_u_w = tf;
    tf = p1_v_w;
    p1_v_w = p2_v_w;
    p2_v_w = tf;
  }

  if (p0.y == p2.y)
    return;

  int32_t dy_total = p2.y - p0.y;
  if (dy_total == 0)
    return;

  for (int y = p0.y; y <= p2.y; y++) {
    if (y < 0 || y >= SCREEN_HEIGHT)
      continue;

    int is_top_half = (y < p1.y || p1.y == p2.y);
    int32_t dy_segment = is_top_half ? (p1.y - p0.y) : (p2.y - p1.y);

    float t_total = (float)(y - p0.y) / (float)dy_total;
    int16_t ax = p0.x + (int16_t)((float)(p2.x - p0.x) * t_total);
    float au_w = p0_u_w + (p2_u_w - p0_u_w) * t_total;
    float av_w = p0_v_w + (p2_v_w - p0_v_w) * t_total;
    float aw = p0.w + (p2.w - p0.w) * t_total;

    int16_t bx;
    float bu_w, bv_w, bw;

    if (dy_segment != 0) {
      float t_segment = is_top_half ? (float)(y - p0.y) / (float)dy_segment
                                    : (float)(y - p1.y) / (float)dy_segment;
      if (is_top_half) {
        bx = p0.x + (int16_t)((float)(p1.x - p0.x) * t_segment);
        bu_w = p0_u_w + (p1_u_w - p0_u_w) * t_segment;
        bv_w = p0_v_w + (p1_v_w - p0_v_w) * t_segment;
        bw = p0.w + (p1.w - p0.w) * t_segment;
      } else {
        bx = p1.x + (int16_t)((float)(p2.x - p1.x) * t_segment);
        bu_w = p1_u_w + (p2_u_w - p1_u_w) * t_segment;
        bv_w = p1_v_w + (p2_v_w - p1_v_w) * t_segment;
        bw = p1.w + (p2.w - p1.w) * t_segment;
      }
    } else {
      bx = is_top_half ? p1.x : p2.x;
      bu_w = is_top_half ? p1_u_w : p2_u_w;
      bv_w = is_top_half ? p1_v_w : p2_v_w;
      bw = is_top_half ? p1.w : p2.w;
    }

    if (ax > bx) {
      int16_t tmp_x = ax;
      ax = bx;
      bx = tmp_x;
      float tmp;
      tmp = au_w;
      au_w = bu_w;
      bu_w = tmp;
      tmp = av_w;
      av_w = bv_w;
      bv_w = tmp;
      tmp = aw;
      aw = bw;
      bw = tmp;
    }

    int16_t start_x = (ax < 0) ? 0 : ax;
    int16_t end_x = (bx >= SCREEN_WIDTH) ? (SCREEN_WIDTH - 1) : bx;
    int32_t span_width = bx - ax;

    if (start_x > end_x || span_width <= 0)
      continue;

    uint8_t *dest = MyFrameBuffer + (y * SCREEN_WIDTH) + start_x;

    float t_start = (float)(start_x - ax) / (float)span_width;
    float start_u_w = au_w + (bu_w - au_w) * t_start;
    float start_v_w = av_w + (bv_w - av_w) * t_start;
    float start_w = aw + (bw - aw) * t_start;
    int32_t start_u = (start_w > 0.00001f) ? (int32_t)(start_u_w / start_w) : 0;
    int32_t start_v = (start_w > 0.00001f) ? (int32_t)(start_v_w / start_w) : 0;

    float t_end = (float)(end_x - ax) / (float)span_width;
    float end_u_w = au_w + (bu_w - au_w) * t_end;
    float end_v_w = av_w + (bv_w - av_w) * t_end;
    float end_w = aw + (bw - aw) * t_end;
    int32_t end_u = (end_w > 0.00001f) ? (int32_t)(end_u_w / end_w) : 0;
    int32_t end_v = (end_w > 0.00001f) ? (int32_t)(end_v_w / end_w) : 0;

    int32_t fixed_u = start_u << 16;
    int32_t fixed_v = start_v << 16;

    int32_t pixels_count = end_x - start_x;
    int32_t step_u = 0, step_v = 0;
    if (pixels_count > 0) {

      step_u = (int32_t)(((int64_t)(end_u - start_u) << 16) / pixels_count);
      step_v = (int32_t)(((int64_t)(end_v - start_v) << 16) / pixels_count);
    }

    for (int16_t x = start_x; x <= end_x; x++) {
      int32_t u = fixed_u >> 16;
      int32_t v = fixed_v >> 16;

      if (u < 0)
        u = 0;
      else if (u >= TEXTURE_SIZE)
        u = TEXTURE_SIZE - 1;
      if (v < 0)
        v = 0;
      else if (v >= TEXTURE_SIZE)
        v = TEXTURE_SIZE - 1;

      *dest++ = texture[(v << 5) + u];

      fixed_u += step_u;
      fixed_v += step_v;
    }
  }
}

void renderCube(int16_t degX, int16_t degY, int16_t degZ) {
  Vertex2D projected[8];

  float radX = (float)degX * 0.0174532925f;
  float radY = (float)degY * 0.0174532925f;
  float radZ = (float)degZ * 0.0174532925f;

  float sx = sinf(radX), cx = cosf(radX);
  float sy = sinf(radY), cy = cosf(radY);
  float sz = sinf(radZ), cz = cosf(radZ);

  for (int i = 0; i < 8; i++) {
    Vector3 v = CUBE_VERTICES[i];

    float x_r = v.x;
    float y_r = v.y * cx - v.z * sx;
    float z_r = v.y * sx + v.z * cx;

    float x_ry = x_r * cy + z_r * sy;
    float y_ry = y_r;
    float z_r2 = -x_r * sy + z_r * cy;

    float x_final = x_ry * cz - y_ry * sz;
    float y_final = x_ry * sz + y_ry * cz;
    float z_final = z_r2;

    float z_offset = z_final + 100.0f;
    if (z_offset < 10.0f)
      z_offset = 10.0f;

    projected[i].x = (SCREEN_WIDTH / 2) +
                     (int16_t)((x_final * (float)SCREEN_HEIGHT) / z_offset);
    projected[i].y = (SCREEN_HEIGHT / 2) +
                     (int16_t)((y_final * (float)SCREEN_HEIGHT) / z_offset);
    projected[i].w = 1.0f / z_offset;
  }

  for (int i = 0; i < 6; i++) {
    uint8_t idx0 = CUBE_FACES[i][0];
    uint8_t idx1 = CUBE_FACES[i][1];
    uint8_t idx2 = CUBE_FACES[i][2];
    uint8_t idx3 = CUBE_FACES[i][3];

    Vertex2D p0 = projected[idx0];
    Vertex2D p1 = projected[idx1];
    Vertex2D p2 = projected[idx2];
    Vertex2D p3 = projected[idx3];

    int32_t backface = ((int32_t)(p1.x - p0.x) * (int32_t)(p2.y - p0.y)) -
                       ((int32_t)(p1.y - p0.y) * (int32_t)(p2.x - p0.x));

    if (backface > 0) {

      p0.u = FACE_UV[0][0];
      p0.v = FACE_UV[0][1];
      p1.u = FACE_UV[1][0];
      p1.v = FACE_UV[1][1];
      p2.u = FACE_UV[2][0];
      p2.v = FACE_UV[2][1];
      p3.u = FACE_UV[3][0];
      p3.v = FACE_UV[3][1];

      drawTexturedTriangle(p0, p1, p2, Textures[i]);
      drawTexturedTriangle(p0, p2, p3, Textures[i]);
    }
  }
}

bool check_keypress(uint8_t *code, bool *l, bool *r, bool *u, bool *d,
                    bool *space) {
  uint32_t kcode = tcm_syscall_read_keyboard();
  if (kcode == 0)
    return false;

  bool press = __TCM_KEYBOARD_PRESS(kcode);
  uint8_t key = __TCM_KEYBOARD_KCODE(kcode);
  switch (key) {
  case __TCM_KEY_CODE_LEFT:
    *l = press;
    return true;
  case __TCM_KEY_CODE_RIGHT:
    *r = press;
    return true;
  case __TCM_KEY_CODE_UP:
    *u = press;
    return true;
  case __TCM_KEY_CODE_DOWN:
    *d = press;
    return true;
  case 0x20:
    *space = press;
    return true;

  default:
    *code = key;
    return true;
  }
}

void main_loop(uint8_t *VRAM) {
  int16_t angleX = 0;
  int16_t angleY = 0;
  int16_t angleZ = 0;

  bool leftpress = false;
  bool rightpress = false;
  bool downpress = false;
  bool uppress = false;
  bool spacepress = false;
  uint8_t kcode;

  int16_t a = rand() % 3 + 1;
  int16_t b = rand() % 3 + 1;
  int16_t c = rand() % 3 + 1;

  while (1) {
    memset(MyFrameBuffer, 0x00, BUFFER_SIZE);
    renderCube(angleX, angleY, angleZ);
    memcpy(VRAM, MyFrameBuffer, BUFFER_SIZE);

    kcode = 0;
    check_keypress(&kcode, &leftpress, &rightpress, &uppress, &downpress,
                   &spacepress);
    if (kcode == 'q' || kcode == 'Q')
      break;

    int16_t xd = 0, yd = 0, zd = 0;
    if (leftpress) {
      yd = -3;
      a = b = c = 0;
    } else if (rightpress) {
      yd = 3;
      a = b = c = 0;
    } else if (downpress) {
      xd = -3;
      a = b = c = 0;
    } else if (uppress) {
      xd = 3;
      a = b = c = 0;
    } else if (spacepress) {
      zd = 3;
      a = b = c = 0;
    } else {
      xd = a;
      yd = b;
      zd = c;
    }

    angleX = (angleX + xd) % 360;
    angleY = (angleY + yd) % 360;
    angleZ = (angleZ + zd) % 360;
  }
}

void rotating_cube() {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  srand(time(NULL));

  void *VRAM = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, SCREEN_WIDTH);
  tcm_pixel_console_clear();

  main_loop(VRAM);
}
