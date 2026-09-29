/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_types.h / SDL_stdinc.h subset.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_types_h
#define _SDL_types_h

#include <stdint.h>
#include <stddef.h>

#include "begin_code.h"
/* @filebegin @ */
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef int8_t      Sint8;
typedef uint8_t     Uint8;
typedef int16_t     Sint16;
typedef uint16_t    Uint16;
typedef int32_t     Sint32;
typedef uint32_t    Uint32;
typedef int64_t     Sint64;
typedef uint64_t    Uint64;

#ifndef SDL_bool
typedef enum {
    SDL_FALSE = 0,
    SDL_TRUE  = 1
} SDL_bool;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"
/* @fileend @ */

#endif /* _SDL_types_h */
