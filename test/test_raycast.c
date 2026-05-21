//
// Created by zhangjiantao on 2026/5/18.
//

// 射线投射算法实现的光照渲染
#include <dev/syscall.h>
#include <dev/console.h>
#include <dev/seven_segment_display.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 80
#define HEIGHT 48

typedef struct {
  float x, y, z;
} Vec3;

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline void vec3_normalize(Vec3 *v) {
  float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
  if (len > 0.0f) {
    v->x /= len;
    v->y /= len;
    v->z /= len;
  }
}

void render_shaded_sphere(void) {
  Vec3 sphere_center = {0.0f, 0.0f, 3.5f};
  float sphere_radius = 2.2f;

  Vec3 light_dir = {1.0f, 1.0f, -1.0f};
  vec3_normalize(&light_dir);

  float aspect_ratio = (float)WIDTH / (float)HEIGHT;
  // float pixel_aspect = 0.5f;

  uint32_t line[2][80] = {0};

  for (int y = 0; y < HEIGHT; y++) {
    if ((y & 1) == 0)
      memset(line, 0, sizeof(line));

    for (int x = 0; x < WIDTH; x++) {
      tcm_seven_segment_display_hexadecimal(y, x);

      float px = ((float)x / WIDTH) * 2.0f - 1.0f;
      float py = ((float)y / HEIGHT) * 2.0f - 1.0f;

      px *= aspect_ratio;// * pixel_aspect;
      py = -py;

      Vec3 ray_dir = {px, py, 1.0f};
      vec3_normalize(&ray_dir);

      Vec3 o_minus_c = vec3_sub((Vec3){0, 0, 0}, sphere_center);
      float b = 2.0f * vec3_dot(ray_dir, o_minus_c);
      float c =
          vec3_dot(o_minus_c, o_minus_c) - (sphere_radius * sphere_radius);
      float discriminant = b * b - 4.0f * c;

      if (discriminant >= 0.0f) {
        float t = (-b - sqrtf(discriminant)) / 2.0f;

        if (t > 0.0f) {
          Vec3 hit_point = {ray_dir.x * t, ray_dir.y * t, ray_dir.z * t};

          Vec3 normal = vec3_sub(hit_point, sphere_center);
          vec3_normalize(&normal);

          float intensity = vec3_dot(normal, light_dir);
          if (intensity < 0.0f)
            intensity = 0.0f;

          intensity += 0.05f;
          if (intensity > 1.0f)
            intensity = 1.0f;

          uint8_t color = (uint8_t)(intensity * 255.0f);
          if ((y & 1) == 0) {
            line[0][x] = FG_COLOR(color, color, color);
          } else {
            line[1][x] = BG_COLOR(color, color, color);
          }
        }
      }
    }

    if (y & 1) {
      uint32_t offset = (y / 2) * 20;
      for (int x = 0; x < 20; x++) {
        tcm_syscall_console_set_color(line[1][x * 4 + 0], line[0][x * 4 + 0]);
        tcm_graphic_console_write(GRAPHIC_0, offset + x, 1, 0xdfdfdfdf);
        tcm_syscall_console_set_color(line[1][x * 4 + 1], line[0][x * 4 + 1]);
        tcm_graphic_console_write(GRAPHIC_0, offset + x, 2, 0xdfdfdfdf);
        tcm_syscall_console_set_color(line[1][x * 4 + 2], line[0][x * 4 + 2]);
        tcm_graphic_console_write(GRAPHIC_0, offset + x, 4, 0xdfdfdfdf);
        tcm_syscall_console_set_color(line[1][x * 4 + 3], line[0][x * 4 + 3]);
        tcm_graphic_console_write(GRAPHIC_0, offset + x, 8, 0xdfdfdfdf);
      }
    }
  }
}

int main() {
  render_shaded_sphere();
  return 0;
}
