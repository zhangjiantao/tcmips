//
// Created by zhangjiantao on 26-5-8.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <time.h>

#define BOARD_WIDTH 12
#define BOARD_HEIGHT 22

// 定义方块形状的最大尺寸 (4x4)
typedef struct {
  uint8_t data[4][4];
  int size;
} Shape;

typedef struct {
  uint8_t board[BOARD_HEIGHT][BOARD_WIDTH]; // 0:空, 1:固定, 2:活动
  int score;
  bool game_over;

  Shape current_shape;
  unsigned current_shape_id;

  int p_x, p_y; // 当前方块坐标
  Shape next_shape;
  unsigned next_shape_id;
} TetrisGame;

// 接口函数
void tetris_init(TetrisGame *game);
bool tetris_move(TetrisGame *game, int dx);
void tetris_rotate(TetrisGame *game);
void tetris_update(TetrisGame *game); // 逻辑步进（下落）
void tetris_get_render_matrix(TetrisGame *game,
                              uint8_t output[BOARD_HEIGHT][BOARD_WIDTH]);

// 7种经典方块定义
static const Shape SHAPES[7] = {
    {{{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 4}, // I
    {{{1, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 2}, // O
    {{{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3}, // T
    {{{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3}, // S
    {{{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3}, // Z
    {{{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3}, // J
    {{{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3}  // L
};

uint32_t FCOLORS[] = {
    FG_COLOR(0, 0, 0),          FG_COLOR(0x80, 0x80, 0x80),
    FG_COLOR(0x55, 0xcc, 0xff), FG_COLOR(0xff, 0xdd, 0x55),
    FG_COLOR(0xaa, 0x66, 0xff), FG_COLOR(0xff, 0xaa, 0x33),
    FG_COLOR(0x33, 0x66, 0xff), FG_COLOR(0x66, 0xdd, 0x66),
    FG_COLOR(0xff, 0x55, 0x55),
};
uint32_t BCOLORS[] = {
    FG_COLOR(0, 0, 0),          BG_COLOR(0x80, 0x80, 0x80),
    BG_COLOR(0x55, 0xcc, 0xff), BG_COLOR(0xff, 0xdd, 0x55),
    BG_COLOR(0xaa, 0x66, 0xff), BG_COLOR(0xff, 0xaa, 0x33),
    BG_COLOR(0x33, 0x66, 0xff), BG_COLOR(0x66, 0xdd, 0x66),
    BG_COLOR(0xff, 0x55, 0x55),
};

static Shape get_random_shape(unsigned *id) {
  *id = rand() % 7;
  return SHAPES[*id];
}

static bool check_collision(TetrisGame *game, Shape *shape, int nx, int ny) {
  for (int r = 0; r < shape->size; r++) {
    for (int c = 0; c < shape->size; c++) {
      if (shape->data[r][c]) {
        int target_x = nx + c;
        int target_y = ny + r;
        if (target_x < 0 || target_x >= BOARD_WIDTH || target_y >= BOARD_HEIGHT)
          return true;
        if (target_y >= 0 && game->board[target_y][target_x] == 1)
          return true;
      }
    }
  }
  return false;
}

void tetris_init(TetrisGame *game) {
  memset(game->board, 0, sizeof(game->board));
  srand(tcm_syscall_get_timestamp_micro());
  game->score = 0;
  game->game_over = false;
  game->next_shape = get_random_shape(&game->next_shape_id);
  // 初始生成
  game->current_shape = get_random_shape(&game->current_shape_id);
  game->p_x = BOARD_WIDTH / 2 - 2;
  game->p_y = 0;
}

void tetris_rotate(TetrisGame *game) {
  Shape rotated = {.size = game->current_shape.size};
  for (int r = 0; r < rotated.size; r++) {
    for (int c = 0; c < rotated.size; c++) {
      rotated.data[c][rotated.size - 1 - r] = game->current_shape.data[r][c];
    }
  }
  if (!check_collision(game, &rotated, game->p_x, game->p_y)) {
    game->current_shape = rotated;
  }
}

bool tetris_move(TetrisGame *game, int dx) {
  if (!check_collision(game, &game->current_shape, game->p_x + dx, game->p_y)) {
    game->p_x += dx;
    return true;
  }
  return false;
}

static void freeze_and_clear(TetrisGame *game) {
  // 固定方块
  for (int r = 0; r < game->current_shape.size; r++) {
    for (int c = 0; c < game->current_shape.size; c++) {
      if (game->current_shape.data[r][c]) {
        int ty = game->p_y + r;
        int tx = game->p_x + c;
        if (ty >= 0 && ty < BOARD_HEIGHT)
          game->board[ty][tx] = 1;
      }
    }
  }

  // 简单消行逻辑
  for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < BOARD_WIDTH; c++) {
      if (game->board[r][c] == 0) {
        full = false;
        break;
      }
    }
    if (full) {
      game->score += 100;
      for (int move_r = r; move_r > 0; move_r--) {
        memcpy(game->board[move_r], game->board[move_r - 1], BOARD_WIDTH);
      }
      memset(game->board[0], 0, BOARD_WIDTH);
      r++; // 重新检查这一行
    }
  }

  // 生成下一个
  game->current_shape = game->next_shape;
  game->current_shape_id = game->next_shape_id;
  game->next_shape = get_random_shape(&game->next_shape_id);
  game->p_x = BOARD_WIDTH / 2 - 2;
  game->p_y = 0;
  if (check_collision(game, &game->current_shape, game->p_x, game->p_y)) {
    game->game_over = true;
  }
}

void tetris_update(TetrisGame *game) {
  if (game->game_over)
    return;
  if (!check_collision(game, &game->current_shape, game->p_x, game->p_y + 1)) {
    game->p_y++;
  } else {
    freeze_and_clear(game);
  }
}

void tetris_get_render_matrix(TetrisGame *game,
                              uint8_t output[BOARD_HEIGHT][BOARD_WIDTH]) {
  // 1. 拷贝背景
  memcpy(output, game->board, sizeof(game->board));
  // 2. 叠加当前活动方块
  if (!game->game_over) {
    for (int r = 0; r < game->current_shape.size; r++) {
      for (int c = 0; c < game->current_shape.size; c++) {
        if (game->current_shape.data[r][c]) {
          int ty = game->p_y + r;
          int tx = game->p_x + c;
          if (ty >= 0 && ty < BOARD_HEIGHT && tx >= 0 && tx < BOARD_WIDTH) {
            output[ty][tx] = 2 + game->current_shape_id; // 活动方块标识
          }
        }
      }
    }
  }
}

#define VMEM_BASE 0

static void render(TetrisGame *game) {
  uint8_t matrix[BOARD_HEIGHT][BOARD_WIDTH];
  tetris_get_render_matrix(game, matrix);

  for (unsigned row = 0; row < BOARD_HEIGHT; row++) {
    for (unsigned col = 0; col < BOARD_WIDTH; col++) {
      uint32_t fg = 0, bg = 0;
      uint32_t v;
      uint32_t mask = (col & 1) ? 0xc0000 : 0x30000;
      v = matrix[row][col];
      fg = FCOLORS[v];
      bg = BCOLORS[v];
      tcm_syscall_console_set_color(bg, fg);
      tcm_syscall_console_write(
          mask | ((row + 1) * 20 + (col / 2) + 7 + VMEM_BASE), 0xdfdfdfdf);
    }
  }
}

unsigned int_sqrt(unsigned long x) {
  unsigned long temp = 0;
  unsigned v_bit = 15;
  unsigned n = 0;
  unsigned b = 0x8000;
  if (x <= 1)
    return x;
  do {
    temp = ((n << 1) + b) << (v_bit--);
    if (x >= temp) {
      n += b;
      x -= temp;
    }
  } while (b >>= 1);
  return n;
}

void tetris_game(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  tcm_syscall_console_set_base(VMEM_BASE);

  uint8_t r = tcm_syscall_get_timestamp_micro() % 40;
  uint8_t g = tcm_syscall_get_timestamp_micro() % 100;
  uint8_t b = tcm_syscall_get_timestamp_micro() % 60;
  for (int row = 0; row < 24; ++row) {
    for (int col = 0; col < 20; ++col) {
      uint8_t rr = r + (row + col) * 4;
      uint8_t gg = g + (row + col) * 4;
      uint8_t bb = b + (row + col) * 4;
      tcm_console_set_color(rr, gg, bb, rr, gg, bb);
      tcm_graphic_console_write(GRAPHIC_0, row * 20 + col, 0xf, 0xdfdfdfdf);
    }
  }


  TetrisGame game;
  tetris_init(&game);

  bool quit = false;
  while (!quit) {
    uint32_t t = tcm_syscall_get_timestamp_milli();
    while (tcm_syscall_get_timestamp_milli() - t < 500) {
      uint32_t k = tcm_keyboard_get_code();
      if (!k || (k & 0x100))
        continue;

      switch (k) {
      case 'w':
        tetris_rotate(&game);
        break;
      case 'a':
        tetris_move(&game, -1);
        break;
      case 's':
        tetris_update(&game);
        break;
      case 'd':
        tetris_move(&game, 1);
        break;
      case 'q':
      case 'Q':
        quit = true;
        break;
      default:
        break;
      }
    }
    tetris_update(&game);
    render(&game);
  }

  tcm_syscall_console_set_color(0, 0xffffff00);
  tcm_syscall_seven_segment_display_upper(0, 0);
  tcm_syscall_seven_segment_display_lower(0, 0);
  printf(">>> Exiting...\n");
  tcm_graphic_console_clear(GRAPHIC_0);
}
