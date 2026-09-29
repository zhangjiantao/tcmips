/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file provides the SDL stdinc macro subset without pulling in
  the SDL2 public headers, which would collide with our SDL1 type definitions.
*/

#ifndef _SDL_stdinc_h
#define _SDL_stdinc_h

#include "SDL_types.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Memory allocation.
 *
 * When TCM_SDL_DEBUG is defined (see tcm_config.h, controlled by the
 * project-level TCM_SDL_DEBUG switch in the root CMakeLists.txt), the SDL1
 * layer provides real wrapper functions on the `SDL_malloc` / `SDL_calloc`
 * / `SDL_realloc` / `SDL_free` / `SDL_aligned_alloc` / `SDL_aligned_free`
 * symbols (implemented in tcmips_sdl_state.c).  Any caller — including the
 * SDL compatibility layer internals and linked user code — resolves to
 * these strong symbols, so every live allocation is counted and
 * `tcmips_sdl_debug_alloc_count()` can report leaks.  When the flag is
 * off, the symbols reduce to the upstream libc aliases via these #defines
 * (zero overhead).
 *
 * SDL_strdup / SDL_wcsdup stay as libc `strdup` / `wcsdup` aliases in both
 * modes: character-buffer management is the user's responsibility and is
 * deliberately not folded into the malloc-tracking path. */
#ifdef TCM_SDL_DEBUG
extern void  *SDL_malloc(size_t size);
extern void  *SDL_calloc(size_t nmemb, size_t size);
extern void  *SDL_realloc(void *mem, size_t size);
extern void   SDL_free(void *mem);
extern void  *SDL_aligned_alloc(size_t alignment, size_t size);
extern void   SDL_aligned_free(void *mem);
extern size_t tcmips_sdl_debug_alloc_count(void);
#else
#define SDL_malloc      malloc
#define SDL_calloc      calloc
#define SDL_realloc     realloc
#define SDL_free        free

#define SDL_aligned_alloc  aligned_alloc
#define SDL_aligned_free   free
#endif /* TCM_SDL_DEBUG */

/* String functions */
#define SDL_strdup      strdup
#define SDL_strlcpy     strlcpy
#define SDL_strlcat     strlcat
#define SDL_strlen      strlen
#define SDL_strcmp      strcmp
#define SDL_strncmp     strncmp
#define SDL_strcasecmp  strcasecmp
#define SDL_strncasecmp strncasecmp
#define SDL_strchr      strchr
#define SDL_strrchr     strrchr
#define SDL_strstr      strstr
#define SDL_strtok_r    strtok_r
#define SDL_sscanf      sscanf
#define SDL_vsnprintf   vsnprintf
#define SDL_snprintf    snprintf

/* Math -- fallback to libm via compiler */
#define SDL_fabs        fabs
#define SDL_floor       floor
#define SDL_ceil        ceil
#define SDL_pow         pow
#define SDL_sqrt        sqrt
#define SDL_sin         sin
#define SDL_cos         cos
#define SDL_tan         tan
#define SDL_atan        atan
#define SDL_atan2       atan2
#define SDL_log         log
#define SDL_exp         exp
#define SDL_atof        atof

/* Misc */
#define SDL_memset      memset
#define SDL_memcpy      memcpy
#define SDL_memmove     memmove
#define SDL_memcmp      memcmp
#define SDL_getenv      getenv
#define SDL_qsort       qsort
#define SDL_abs(x)      ((x) < 0 ? -(x) : (x))
#define SDL_min(x, y)   (((x) < (y)) ? (x) : (y))
#define SDL_max(x, y)   (((x) > (y)) ? (x) : (y))

#endif /* _SDL_stdinc_h */
