/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 begin_code.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_begin_code_h
#define _SDL_begin_code_h

/* Bring in the project-level build configuration first so that all SDL1
 * headers below can react to TCMIPS-specific build flags such as
 * TCM_SDL_DEBUG.  Having this #include on the most-leaf header guarantees
 * every SDL1 header that includes SDL_types.h (which is "include first" in
 * the chain) automatically inherits the flag without redeclaration. */
#include <tcm_config.h>

#ifdef __cplusplus
#define SDL_DECLSPEC
#else
#define SDL_DECLSPEC
#endif

#ifndef SDLCALL
#if defined(__WIN32__) && !defined(__GNUC__)
#define SDLCALL __cdecl
#else
#define SDLCALL
#endif
#endif /* SDLCALL */

#ifndef SDL_INLINE
#if defined(__GNUC__)
#define SDL_INLINE __inline__
#elif defined(_MSC_VER)
#define SDL_INLINE __inline
#else
#define SDL_INLINE inline
#endif
#endif /* SDL_INLINE */

#endif /* _SDL_begin_code_h */
