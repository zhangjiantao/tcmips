//
// Created by zhangjiantao on 2026/5/18.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <tcm_config.h>

void show_welcome(void) {
  printf("TCMIPS 演示控制台\n");
  printf("版本：%s\n", TCMIPS_VERSION_STRING);
  printf("构建：%s\n", __DATE__ " " __TIME__);
  printf("输入 'help' 查看可用命令。\n\n");
}

void handle_help(void) {
  show_welcome();
  printf("可用命令列表：\n"
         " help       - 显示此帮助菜单\n"
         " test       - 运行自测套件\n"
         " clear      - 清空屏幕\n"
         " time       - 显示当前系统时间\n"
         " cube       - 3D 旋转立方体演示\n"
         " raycast    - 光线投射法 3D 光影渲染演示\n"
         " part       - 粒子模拟演示\n"
         " mand       - 渲染曼德博集图像\n"
         " python     - 进入 MicroPython 交互式 REPL 会话\n"
         " exit       - 退出 REPL 会话\n");
}

static void handle_clear(void) { tcm_text_console_clear(); }

static void handle_time(void) {
  char buf[128];
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %A %z (%Z)", lt);
  printf(">>> %s\n", buf);
}

static void handle_cube(void) {
  extern void rotating_cube();
  rotating_cube();
}

static void handle_raycast(void) {
  extern void raycast_softfp(void);
  raycast_softfp();
}

static void handle_particle(void) {
  extern void particle_simulation(void);
  particle_simulation();
}

static void handle_mandelbrotset(void) {
  extern void mandelbrotset(void);
  mandelbrotset();
}

static void handle_basictest(int argc, char *argv[]) {
  extern int basictest(int, char *[]);
  basictest(argc, argv);
}

static void handle_python(void) {
  extern void micropython_repl(void);
  tcm_text_console_mode(false, false);
  micropython_repl();
  tcm_text_console_mode(true, true);
}

static void repl(void) {
#define MAX_ARGS 8
#define MAX_INPS 256
  char Buffer[MAX_INPS] = "cube";
  bool init = false;
  bool running = true;

  while (running) {
    fflush(stdout);
    tcm_text_console_set_color(0, 255, 0, 0, 0, 0);
    printf("[TCMIPS] # ");
    fflush(stdout);
    tcm_text_console_set_color(255, 255, 255, 0, 0, 0);
    fflush(stdout);

    if (init) {
      if (fgets(Buffer, MAX_INPS, stdin) == NULL) {
        break;
      }
    } else {
      printf("\n");
    }
    init = true;
    Buffer[strcspn(Buffer, "\r\n")] = '\0';

    char *argv[MAX_ARGS];
    int argc = 0;
    char *token = strtok(Buffer, " \t");
    while (token != NULL && argc < MAX_ARGS) {
      argv[argc++] = token;
      token = strtok(NULL, " \t");
    }

    if (argc == 0)
      continue;

    char *cmd = argv[0];

    if (strlen(cmd) == 0)
      continue;

    if (strcmp(cmd, "exit") == 0) {
      running = false;
      printf("正在退出，再见。\n");
    } else if (strcmp(cmd, "help") == 0) {
      handle_help();
    } else if (strcmp(cmd, "clear") == 0) {
      handle_clear();
    } else if (strcmp(cmd, "time") == 0) {
      handle_time();
    } else if (strcmp(cmd, "cube") == 0) {
      handle_cube();
    } else if (strcmp(cmd, "raycast") == 0) {
      handle_raycast();
    } else if (strcmp(cmd, "part") == 0) {
      handle_particle();
    } else if (strcmp(cmd, "mand") == 0) {
      handle_mandelbrotset();
    } else if (strcmp(cmd, "test") == 0) {
      handle_basictest(argc, argv);
    } else if (strcmp(cmd, "python") == 0) {
      handle_python();
    } else {
      printf("错误：未知命令 '%s'，请输入 'help' 查看帮助。\n", cmd);
    }
  }
}

int main() {
  show_welcome();

  repl();

  return 0;
}