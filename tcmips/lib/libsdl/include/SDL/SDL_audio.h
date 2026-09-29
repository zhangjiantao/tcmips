/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_audio.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_audio_h
#define _SDL_audio_h

#include "SDL_types.h"
#include "SDL_endian.h"
#include "SDL_rwops.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* A convenience macro for the obtained audio format */
#define AUDIO_U8	0x0008
#define AUDIO_S8	0x8008
#define AUDIO_U16LSB	0x0010
#define AUDIO_S16LSB	0x8010
#define AUDIO_U16MSB	0x1010
#define AUDIO_S16MSB	0x9010
#define AUDIO_U16	AUDIO_U16LSB
#define AUDIO_S16	AUDIO_S16LSB
#define AUDIO_S32LSB	0x8020
#define AUDIO_S32MSB	0x9020
#define AUDIO_S32	AUDIO_S32LSB
#define AUDIO_F32LSB	0x8120
#define AUDIO_F32MSB	0x9120
#define AUDIO_F32	AUDIO_F32LSB
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS	AUDIO_U16LSB
#define AUDIO_S16SYS	AUDIO_S16LSB
#define AUDIO_S32SYS	AUDIO_S32LSB
#define AUDIO_F32SYS	AUDIO_F32LSB
#else
#define AUDIO_U16SYS	AUDIO_U16MSB
#define AUDIO_S16SYS	AUDIO_S16MSB
#define AUDIO_S32SYS	AUDIO_S32MSB
#define AUDIO_F32SYS	AUDIO_F32MSB
#endif

/* Maximum audio mixed volume */
#define SDL_MIX_MAXVOLUME 128

/* The calculated values in the SDL_AudioSpec structure */
typedef struct SDL_AudioSpec {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint32 size;
    void (SDLCALL *callback)(void *userdata, Uint8 *stream, int len);
    void *userdata;
} SDL_AudioSpec;

/* This is the opaque audio conversion structure */
typedef struct SDL_AudioCVT {
    int needed;
    Uint16 src_format;
    Uint16 dst_format;
    double rate_incr;
    Uint8 *buf;
    int len;
    int len_cvt;
    int len_mult;
    double len_ratio;
    void (SDLCALL *filters[10])(struct SDL_AudioCVT *cvt, Uint16 format);
    int filter_index;
} SDL_AudioCVT;

/* The desired audio state -- not implemented on TCMIPS */
typedef enum {
    SDL_AUDIO_STOPPED = 0,
    SDL_AUDIO_PLAYING,
    SDL_AUDIO_PAUSED
} SDL_audiostatus;

SDL_DECLSPEC int SDLCALL SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
SDL_DECLSPEC void SDLCALL SDL_PauseAudio(int pause_on);
SDL_DECLSPEC SDL_audiostatus SDLCALL SDL_GetAudioStatus(void);
SDL_DECLSPEC void SDLCALL SDL_CloseAudio(void);

SDL_DECLSPEC void SDLCALL SDL_LockAudio(void);
SDL_DECLSPEC void SDLCALL SDL_UnlockAudio(void);

SDL_DECLSPEC void SDLCALL SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume);

SDL_DECLSPEC SDL_AudioSpec * SDLCALL SDL_LoadWAV_RW(SDL_RWops *src, int freesrc,
					SDL_AudioSpec *spec,
					Uint8 **audio_buf,
					Uint32 *audio_len);
#define SDL_LoadWAV(file, spec, audio_buf, audio_len) \
	SDL_LoadWAV_RW(SDL_RWFromFile(file, "rb"), 1, spec, audio_buf, audio_len)
SDL_DECLSPEC void SDLCALL SDL_FreeWAV(Uint8 *audio_buf);

SDL_DECLSPEC int SDLCALL SDL_BuildAudioCVT(SDL_AudioCVT *cvt,
									   Uint16 src_format, Uint8 src_channels,
									   int src_rate, Uint16 dst_format,
									   Uint8 dst_channels, int dst_rate);
SDL_DECLSPEC int SDLCALL SDL_ConvertAudio(SDL_AudioCVT *cvt);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_audio_h */
