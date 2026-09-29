/* SDL compatibility layer: audio (dummy implementation). */

#include "tcmips_sdl_internal.h"

#include <string.h>

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  if (!desired)
    return -1;
  if (g_tcmips_sdl.audio_opened) {
    SDL_SetError("Audio already opened");
    return -1;
  }

  SDL_memcpy(&g_tcmips_sdl.audio_desired, desired, sizeof(SDL_AudioSpec));
  if (obtained) {
    SDL_memcpy(obtained, desired, sizeof(SDL_AudioSpec));
  }

  g_tcmips_sdl.audio_obtained = g_tcmips_sdl.audio_desired;
  g_tcmips_sdl.audio_opened = 1;
  g_tcmips_sdl.audio_paused = 1;
  return 0;
}

void SDL_CloseAudio(void) {
  g_tcmips_sdl.audio_opened = 0;
  g_tcmips_sdl.audio_paused = 1;
  SDL_memset(&g_tcmips_sdl.audio_desired, 0, sizeof(SDL_AudioSpec));
  SDL_memset(&g_tcmips_sdl.audio_obtained, 0, sizeof(SDL_AudioSpec));
}

void SDL_PauseAudio(int pause_on) {
  if (!g_tcmips_sdl.audio_opened)
    return;
  g_tcmips_sdl.audio_paused = pause_on ? 1 : 0;
}

SDL_audiostatus SDL_GetAudioStatus(void) {
  if (!g_tcmips_sdl.audio_opened)
    return SDL_AUDIO_STOPPED;
  if (g_tcmips_sdl.audio_paused)
    return SDL_AUDIO_PAUSED;
  return SDL_AUDIO_PLAYING;
}

void SDL_LockAudio(void) {}
void SDL_UnlockAudio(void) {}

void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume) {
  if (!dst || !src || len == 0)
    return;
  if (volume == SDL_MIX_MAXVOLUME) {
    SDL_memcpy(dst, src, len);
    return;
  }
  /* Simple linear attenuation for U8 samples. */
  for (Uint32 i = 0; i < len; ++i) {
    Sint16 v = (Sint16)dst[i] +
               (Sint16)(((int)src[i] - 128) * volume / SDL_MIX_MAXVOLUME);
    if (v > 255)
      v = 255;
    if (v < 0)
      v = 0;
    dst[i] = (Uint8)v;
  }
}

SDL_AudioSpec *SDL_LoadWAV_RW(SDL_RWops *src, int freesrc, SDL_AudioSpec *spec,
                              Uint8 **audio_buf, Uint32 *audio_len) {
  (void)spec;
  (void)audio_buf;
  (void)audio_len;
  if (freesrc && src)
    SDL_RWclose(src);
  TCMIPS_SDL_STUB_MSG("SDL_LoadWAV_RW");
  return NULL;
}

void SDL_FreeWAV(Uint8 *audio_buf) { SDL_free(audio_buf); }

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels,
                      int src_rate, Uint16 dst_format, Uint8 dst_channels,
                      int dst_rate) {
  (void)cvt;
  (void)src_format;
  (void)src_channels;
  (void)src_rate;
  (void)dst_format;
  (void)dst_channels;
  (void)dst_rate;
  TCMIPS_SDL_STUB_MSG("SDL_BuildAudioCVT");
  return -1;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt) {
  (void)cvt;
  TCMIPS_SDL_STUB_MSG("SDL_ConvertAudio");
  return -1;
}
