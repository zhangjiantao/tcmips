/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_error.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_error_h
#define _SDL_error_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
typedef enum {
    SDL_ENOMEM,
    SDL_EFREAD,
    SDL_EFWRITE,
    SDL_EFSEEK,
    SDL_UNSUPPORTED,
    SDL_LASTERROR
} SDL_errorcode;

/* Public functions */
SDL_DECLSPEC void SDLCALL SDL_SetError(const char *fmt, ...);
SDL_DECLSPEC char * SDLCALL SDL_GetError(void);
SDL_DECLSPEC void SDLCALL SDL_ClearError(void);

SDL_DECLSPEC void SDLCALL SDL_Error(SDL_errorcode code);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_error_h */
