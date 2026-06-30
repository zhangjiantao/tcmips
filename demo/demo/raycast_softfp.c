//
// Created by zhangjiantao on 2026/5/18.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 240

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

void render_shaded_sphere(uint32_t *VRAM) {
  Vec3 sphere_center = {0.0f, 0.0f, 3.5f};
  float sphere_radius = 2.45f;

  Vec3 light_dir = {-1.0f, 1.0f, -1.0f};
  vec3_normalize(&light_dir);

  float aspect_ratio = (float)WIDTH / (float)HEIGHT;
  

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      tcm_seven_segment_display_hexadecimal(y, x);
      if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
        return;

      float px = ((float)x / WIDTH) * 2.0f - 1.0f;
      float py = ((float)y / HEIGHT) * 2.0f - 1.0f;

      px *= aspect_ratio; 
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

          uint8_t cc = (uint8_t)(intensity * 255.0f);
          VRAM[y * WIDTH + x] = (cc << 24) + (cc << 16) + (cc << 8) + cc;
        }
      }
    }
  }
}

Vec3 vec3_rotate(Vec3 v, float angle_x, float angle_y) {
  float sx = sinf(angle_x);
  float cx = cosf(angle_x);
  float sy = sinf(angle_y);
  float cy = cosf(angle_y);

  
  float x = v.x * cy + v.z * sy;
  float z = -v.x * sy + v.z * cy;

  
  float y = v.y * cx - z * sx;
  z = v.y * sx + z * cx;

  return (Vec3){x, y, z};
}

void raycast_softfp(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  void *VRAM = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, WIDTH);
  tcm_pixel_console_clear();

  tcm_seven_segment_display_hexadecimal(0, 0);
  render_shaded_sphere(VRAM);
}
