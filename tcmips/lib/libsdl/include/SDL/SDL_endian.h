/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_endian.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_endian_h
#define _SDL_endian_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Useful for determining the byte order of the host */
#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321

#if defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL)
#  define SDL_BYTEORDER SDL_LIL_ENDIAN
#else
#  define SDL_BYTEORDER SDL_BIG_ENDIAN
#endif

SDL_DECLSPEC Uint16 SDLCALL SDL_Swap16(Uint16 x);
SDL_DECLSPEC Uint32 SDLCALL SDL_Swap32(Uint32 x);
SDL_DECLSPEC Uint64 SDLCALL SDL_Swap64(Uint64 x);

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SDL_SwapLE16(X)	(X)
#define SDL_SwapLE32(X)	(X)
#define SDL_SwapLE64(X)	(X)
#define SDL_SwapBE16(X)	SDL_Swap16(X)
#define SDL_SwapBE32(X)	SDL_Swap32(X)
#define SDL_SwapBE64(X)	SDL_Swap64(X)
#else
#define SDL_SwapLE16(X)	SDL_Swap16(X)
#define SDL_SwapLE32(X)	SDL_Swap32(X)
#define SDL_SwapLE64(X)	SDL_Swap64(X)
#define SDL_SwapBE16(X)	(X)
#define SDL_SwapBE32(X)	(X)
#define SDL_SwapBE64(X)	(X)
#endif

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_endian_h */
