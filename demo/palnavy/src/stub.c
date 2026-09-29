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
