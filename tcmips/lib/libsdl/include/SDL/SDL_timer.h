/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_timer.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_timer_h
#define _SDL_timer_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

SDL_DECLSPEC Uint32 SDLCALL SDL_GetTicks(void);
SDL_DECLSPEC void SDLCALL SDL_Delay(Uint32 ms);

typedef Uint32 (SDLCALL *SDL_TimerCallback)(Uint32 interval);

SDL_DECLSPEC int SDLCALL SDL_SetTimer(Uint32 interval, SDL_TimerCallback callback);

typedef Uint32 (SDLCALL *SDL_NewTimerCallback)(Uint32 interval, void *param);
typedef struct _SDL_TimerID *SDL_TimerID;

SDL_DECLSPEC SDL_TimerID SDLCALL SDL_AddTimer(Uint32 interval, SDL_NewTimerCallback callback, void *param);
SDL_DECLSPEC SDL_bool SDLCALL SDL_RemoveTimer(SDL_TimerID id);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_timer_h */
