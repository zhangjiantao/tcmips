//
// Created by zhangjiantao on 2026/6/4.
//

#include "NDL.h"
#include "audio.h"

#include "stdlib.h"
#include "util.h"

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

INT AUDIO_OpenDevice(VOID) { return 0; }

VOID AUDIO_CloseDevice(VOID) {}

SDL_AudioSpec *AUDIO_GetDeviceSpec(VOID) { return NULL; }

VOID AUDIO_PlayMusic(INT iNumRIX, BOOL fLoop, INT flFadeTime) {}

BOOL AUDIO_PlayCDTrack(INT iNumTrack) { return FALSE; }

VOID AUDIO_PlaySound(INT iSoundNum) {}

BOOL AUDIO_CD_Available(VOID) { return FALSE; }

static BOOL g_MusicEnabled = FALSE;
static BOOL g_SoundEnabled = FALSE;

VOID AUDIO_EnableMusic(BOOL fEnable) { g_MusicEnabled = fEnable; }

BOOL AUDIO_MusicEnabled(VOID) { return g_MusicEnabled; }

VOID AUDIO_EnableSound(BOOL fEnable) { g_SoundEnabled = fEnable; }

BOOL AUDIO_SoundEnabled(VOID) { return g_SoundEnabled; }

void AUDIO_Lock(void) {}

void AUDIO_Unlock(void) {}

BOOL UTIL_GetScreenSize(DWORD *pdwScreenWidth, DWORD *pdwScreenHeight) {
  if (pdwScreenWidth != NULL && pdwScreenHeight != NULL) {
    *pdwScreenWidth = 320;
    *pdwScreenHeight = 200;
    return TRUE;
  }
  return FALSE;
}

BOOL UTIL_IsAbsolutePath(const char *lpszFileName) { return FALSE; }

int UTIL_Platform_Startup(int argc, char *argv[]) { return 0; }

int UTIL_Platform_Init(int argc, char *argv[]) {
  NDL_Init(0);
  return 0;
}

void UTIL_Platform_Quit(void) {
  NDL_Quit();
  exit(1);
}

#include "resources/game_resources.c"

typedef struct {
  const uint8_t *buffer;
  size_t size;
  size_t pos;
} ROM_FILE;

typedef struct {
  const char *filename;
  const uint8_t *buffer;
  size_t size;
} ROM_FileMap;

extern const uint8_t g_res_RNG_MKF[];
extern const size_t g_res_RNG_MKF_len;
extern const uint8_t g_res_SSS_MKF[];
extern const size_t g_res_SSS_MKF_len;
extern const uint8_t g_res_WORD_DAT[];
extern const size_t g_res_WORD_DAT_len;
extern const uint8_t g_res_M_MSG[];
extern const size_t g_res_M_MSG_len;
extern const uint8_t g_res_WOR16_FON[];
extern const size_t g_res_WOR16_FON_len;
extern const uint8_t g_res_WOR16_ASC[];
extern const size_t g_res_WOR16_ASC_len;
extern const uint8_t g_res_MAP_MKF[];
extern const size_t g_res_MAP_MKF_len;
extern const uint8_t g_res_DATA_MKF[];
extern const size_t g_res_DATA_MKF_len;
extern const uint8_t g_res_ABC_MKF[];
extern const size_t g_res_ABC_MKF_len;
extern const uint8_t g_res_MGO_MKF[];
extern const size_t g_res_MGO_MKF_len;
extern const uint8_t g_res_FBP_MKF[];
extern const size_t g_res_FBP_MKF_len;
extern const uint8_t g_res_FIRE_MKF[];
extern const size_t g_res_FIRE_MKF_len;
extern const uint8_t g_res_BALL_MKF[];
extern const size_t g_res_BALL_MKF_len;
extern const uint8_t g_res_GOP_MKF[];
extern const size_t g_res_GOP_MKF_len;
extern const uint8_t g_res_PAT_MKF[];
extern const size_t g_res_PAT_MKF_len;
extern const uint8_t g_res_RGM_MKF[];
extern const size_t g_res_RGM_MKF_len;
extern const uint8_t g_res_F_MKF[];
extern const size_t g_res_F_MKF_len;

static const ROM_FileMap g_RomFileMap[] = {
    {"RNG.MKF", g_res_RNG_MKF, g_res_RNG_MKF_len},
    {"SSS.MKF", g_res_SSS_MKF, g_res_SSS_MKF_len},
    {"WORD.DAT", g_res_WORD_DAT, g_res_WORD_DAT_len},
    {"M.MSG", g_res_M_MSG, g_res_M_MSG_len},
    {"WOR16.FON", g_res_WOR16_FON, g_res_WOR16_FON_len},
    {"WOR16.ASC", g_res_WOR16_ASC, g_res_WOR16_ASC_len},
    {"MAP.MKF", g_res_MAP_MKF, g_res_MAP_MKF_len},
    {"DATA.MKF", g_res_DATA_MKF, g_res_DATA_MKF_len},
    {"ABC.MKF", g_res_ABC_MKF, g_res_ABC_MKF_len},
    {"MGO.MKF", g_res_MGO_MKF, g_res_MGO_MKF_len},
    {"FBP.MKF", g_res_FBP_MKF, g_res_FBP_MKF_len},
    {"FIRE.MKF", g_res_FIRE_MKF, g_res_FIRE_MKF_len},
    {"BALL.MKF", g_res_BALL_MKF, g_res_BALL_MKF_len},
    {"GOP.MKF", g_res_GOP_MKF, g_res_GOP_MKF_len},
    {"PAT.MKF", g_res_PAT_MKF, g_res_PAT_MKF_len},
    {"RGM.MKF", g_res_RGM_MKF, g_res_RGM_MKF_len},
    {"F.MKF", g_res_F_MKF, g_res_F_MKF_len},
};

#define ROM_FILE_COUNT (sizeof(g_RomFileMap) / sizeof(ROM_FileMap))
#define MAX_OPEN_FILES 32
static ROM_FILE g_VirtualFiles[MAX_OPEN_FILES];

static int my_internal_strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    char c1 = *s1;
    char c2 = *s2;
    if (c1 >= 'a' && c1 <= 'z')
      c1 -= 32;
    if (c2 >= 'a' && c2 <= 'z')
      c2 -= 32;
    if (c1 != c2)
      return (int)(c1 - c2);
    s1++;
    s2++;
  }
  char c1 = *s1;
  char c2 = *s2;
  if (c1 >= 'a' && c1 <= 'z')
    c1 -= 32;
  if (c2 >= 'a' && c2 <= 'z')
    c2 -= 32;
  return (int)(c1 - c2);
}

void *my_rom_fopen(const char *path, const char *mode) {
  if (!path)
    return NULL;

  const char *filename = strrchr(path, '/');
  if (filename) {
    filename++;
  } else {
    filename = path;
  }

  for (size_t i = 0; i < ROM_FILE_COUNT; i++) {
    if (my_internal_strcasecmp(filename, g_RomFileMap[i].filename) == 0) {
      for (int j = 0; j < MAX_OPEN_FILES; j++) {
        if (g_VirtualFiles[j].buffer == NULL) {
          g_VirtualFiles[j].buffer = g_RomFileMap[i].buffer;
          g_VirtualFiles[j].size = g_RomFileMap[i].size;
          g_VirtualFiles[j].pos = 0;
          return &g_VirtualFiles[j];
        }
      }
    }
  }
  return NULL;
}

size_t my_rom_fread(void *ptr, size_t size, size_t count, void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer || size == 0 || count == 0 || !ptr)
    return 0;

  size_t bytes_to_read = size * count;

  if (f->pos + bytes_to_read > f->size) {
    bytes_to_read = f->size - f->pos;
  }

  if (bytes_to_read > 0) {
    memcpy(ptr, f->buffer + f->pos, bytes_to_read);
    f->pos += bytes_to_read;
  }

  size_t ret_count = bytes_to_read / size;

  if (ret_count == 0 && bytes_to_read > 0) {
    ret_count = count;
  }

  return ret_count;
}

int my_rom_fseek(void *stream, long offset, int whence) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer)
    return -1;

  long current_pos = (long)f->pos;
  long file_size = (long)f->size;
  long new_pos = 0;

  if (whence == 0) { // SEEK_SET
    new_pos = offset;
  } else if (whence == 1) { // SEEK_CUR
    new_pos = current_pos + offset;
  } else if (whence == 2) { // SEEK_END
    new_pos = file_size + offset;
  } else {
    return -1;
  }

  if (new_pos < 0 || new_pos > file_size)
    return -1;

  f->pos = (size_t)new_pos;
  return 0;
}

long my_rom_ftell(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  return f ? (long)f->pos : -1;
}

int my_rom_fclose(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (f) {
    f->buffer = NULL;
    f->size = 0;
    f->pos = 0;
  }
  return 0;
}

int my_rom_feof(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer) {
    return 1;
  }
  return (f->pos >= f->size) ? 1 : 0;
}
