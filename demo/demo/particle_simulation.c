//
// Created by zhangjiantao on 2026/6/2.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PARTICLE_NUM 128
#define PARTICLE_SIZE 16

static unsigned char framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

typedef struct {
  float x, y;
  float vx, vy;
  unsigned char color;
  float bounce; 
} Particle;

static Particle particles[PARTICLE_NUM];

unsigned char rgb332(unsigned char r, unsigned char g, unsigned char b) {
  return ((r & 0x07) << 5) | ((g & 0x07) << 2) | (b & 0x03);
}

void clear_fb(void) { memset(framebuffer, 0, sizeof(framebuffer)); }

void draw_pixel(int x, int y, unsigned char color) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    return;
  framebuffer[y * SCREEN_WIDTH + x] = color;
}

void draw_particle(int x, int y, unsigned char color) {
  int radius = PARTICLE_SIZE / 2; 
  int cx = x + radius;            
  int cy = y + radius;            
  int r_sq = radius * radius;     

  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= r_sq) { 
        draw_pixel(cx + dx, cy + dy, color);
      }
    }
  }
}

void init_particles(void) {
  for (int i = 0; i < PARTICLE_NUM; i++) {
    particles[i].x = SCREEN_WIDTH / 2;
    particles[i].y = SCREEN_HEIGHT / 2;

    float angle = (rand() % 360) * 3.14159f / 180.0f;
    float speed = 1.5f + (rand() % 15) / 10.0f;

    particles[i].vx = cosf(angle) * speed;
    particles[i].vy = sinf(angle) * speed;

    unsigned char r = rand() % 8;
    unsigned char g = rand() % 8;
    unsigned char b = rand() % 4;
    particles[i].color = rgb332(r, g, b);

    particles[i].bounce = 0.45f + ((rand() % 35) / 100.0f);
  }
}

void update_particles(void) {
  const float gravity = 0.08f;

  for (int i = 0; i < PARTICLE_NUM; i++) {
    Particle *p = &particles[i];

    p->vy += gravity;
    p->x += p->vx;
    p->y += p->vy;

    if (p->y >= SCREEN_HEIGHT - PARTICLE_SIZE) {
      p->y = SCREEN_HEIGHT - PARTICLE_SIZE;
      p->vy *= -p->bounce; 
      p->vx *= 0.98f;
    }

    
    if (p->x <= 0 || p->x >= SCREEN_WIDTH - PARTICLE_SIZE) {
      p->vx *= -0.8f;
    }

    
    if (p->y > SCREEN_HEIGHT + 50 || p->x < -50 || p->x > SCREEN_WIDTH + 50) {
      p->x = SCREEN_WIDTH / 2;
      p->y = SCREEN_HEIGHT / 2;

      float angle = (rand() % 360) * 3.14159f / 180.0f;
      float speed = 1.5f + (rand() % 15) / 10.0f;
      p->vx = cosf(angle) * speed;
      p->vy = sinf(angle) * speed;
    }
  }
}

void draw_all_particles(void) {
  for (int i = 0; i < PARTICLE_NUM; i++) {
    int x = (int)particles[i].x;
    int y = (int)particles[i].y;
    draw_particle(x, y, particles[i].color);
  }
}

void particle_simulation(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");
  void *VRAM = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, SCREEN_WIDTH);
  tcm_pixel_console_clear();

  srand((unsigned int)time(NULL));
  init_particles();

  static uint32_t frame_counter = 0;
  while (frame_counter < 0x200) {
    if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
      return;

    clear_fb();

    uint32_t t1 = tcm_syscall_get_timestamp_milli();
    update_particles();
    draw_all_particles();
    uint32_t t2 = tcm_syscall_get_timestamp_milli();

    tcm_seven_segment_display_hexadecimal(frame_counter++, t2 - t1);

    memcpy(VRAM, framebuffer, sizeof(framebuffer));
  }
}