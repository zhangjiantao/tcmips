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

// #define WAIT
static void wait(void) {
#ifdef WAIT
  tcm_keyboard_clear();
  while (tcm_syscall_read_keyboard() == 0)
    ;
#endif
}

void show_banner(void) {
  tcm_text_console_write_string(
      "========================================================================"
      "========"
      " On my business card, I am a corporate president.\n"
      " In my mind, I am a game developer.\n"
      " But in my heart, I am a gamer.\n"
      "                                                               -- "
      "Satoru Iwata  "
      "========================================================================"
      "========");
}

void handle_help(void) {
  tcm_text_console_write_string(
      "Available commands:\n"
      "  help                  - Show this help menu\n"
      "  clear                 - Clear screen\n"
      "  time                  - Display the current system time\n"
      "  hash <str>            - Calculate MD5 and SHA-256 checksum\n"
      "  music                 - Play the built-in MIDI music track\n"
      "  image                 - Run graphic dot-matrix rendering demo\n"
      "  video                 - Play the built-in video sequence\n"
      "  tetris                - Play classic Tetris block game\n"
      "  raycast               - 3D light shading rendering via ray casting\n"
      "  python                - Enter MicroPython interactive REPL session\n"
      "  exit                  - Terminate the REPL session\n");
}

static void bytes_to_hex(const uint8_t *hash, char *output, size_t length) {
  const char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < length; i++) {
    output[i * 2] = hex_chars[(hash[i] >> 4) & 0x0f];
    output[i * 2 + 1] = hex_chars[hash[i] & 0x0f];
  }
  output[length * 2] = '\0';
}

static void handle_hash(const char *input) {
  printf(">>> input:\t%s\n", input);
  uint8_t md5_digest[16];
  uint8_t sha_digest[32];
  char output[68];
  extern void tcm_md5_string(const char *, uint8_t[16]);
  tcm_md5_string(input, md5_digest);
  bytes_to_hex(md5_digest, output, 16);
  printf(">>> md5:\t%s\n", output);

  extern void tcm_sha256_string(const char *, uint8_t[32]);
  tcm_sha256_string(input, sha_digest);
  bytes_to_hex(sha_digest, output, 32);
  printf(">>> sha256:\t%s\n", output);
}

static void handle_clear(void) { tcm_text_console_clear(); }

static void handle_time(void) {
  char buf[128];
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %A %z (%Z)", lt);
  printf(">>> %s\n", buf);
}

static void handle_music(void) {
  extern void music_player(void);
  music_player();
}

static void handle_image(void) {
  extern void image_viewer(void);
  image_viewer();
}

static void handle_video(void) {
  extern void video_player(void);
  video_player();
}

static void handle_tetris(void) {
  extern void tetris_game(void);
  tetris_game();
}

static void handle_raycast(void) {
  extern void raycast_softfp(void);
  raycast_softfp();
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
  char Buffer[MAX_INPS];
  bool running = true;

  while (running) {
    tcm_console_set_color(0, 0, 0, 0, 255, 0);
    tcm_text_console_write_string("[tcmips] # ");
    tcm_console_set_color(0, 0, 0, 255, 255, 255);

    if (fgets(Buffer, MAX_INPS, stdin) == NULL)
      break;
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
      tcm_text_console_write_string("Exiting. Goodbye.\n");
    } else if (strcmp(cmd, "help") == 0) {
      handle_help();
    } else if (strcmp(cmd, "clear") == 0) {
      handle_clear();
    } else if (strcmp(cmd, "time") == 0) {
      handle_time();
    } else if (strcmp(cmd, "music") == 0) {
      handle_music();
    } else if (strcmp(cmd, "image") == 0) {
      handle_image();
    } else if (strcmp(cmd, "video") == 0) {
      handle_video();
    } else if (strcmp(cmd, "tetris") == 0) {
      handle_tetris();
    } else if (strcmp(cmd, "raycast") == 0) {
      handle_raycast();
    } else if (strcmp(cmd, "python") == 0) {
      handle_python();
    } else if (strcmp(cmd, "hash") == 0) {
      if (argc < 2) {
        tcm_text_console_write_string("Error: 'hash' command requires an input "
                                      "string.\nUsage: hash <string>\n");
      } else {
        handle_hash(argv[1]);
      }
    } else {
      printf("Error: Unknown command '%s'. Type 'help'.\n", cmd);
    }
  }
}

int main() {
  wait();

  show_banner();

  wait();

  repl();

  return 0;
}