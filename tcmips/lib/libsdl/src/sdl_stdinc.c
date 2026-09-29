/*
 * SDL compatibility layer - self-contained error helpers.
 *
 * The TCMIPS SDL1 layer is deliberately decoupled from the SDL2 layer:
 * it drives the hardware pixel console directly.  As a consequence it must
 * supply its own SDL_SetError/SDL_GetError/SDL_ClearError/SDL_Error
 * implementations so that programs which link only tcmips_sdl (and not
 * tcmips_sdl2) resolve these symbols locally.
 *
 * The SDL1 and SDL2 layers are linked on demand (never both into one
 * executable), so this layer exports the standard public SDL_* symbol
 * names directly, matching the upstream SDL ABI.
 */

#include "tcmips_sdl_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char s_sdl1_error_buf[256];

/* Byte-swap helpers.  SDL_endian.h declares these symbols and the SDL1
 * compatibility macros reference them on big-endian hosts; provide real
 * implementations so programs that link only tcmips_sdl resolve them. */
Uint16 SDL_Swap16(Uint16 x) { return (Uint16)((x >> 8) | (x << 8)); }

Uint32 SDL_Swap32(Uint32 x) {
  return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) |
         ((x & 0x00FF0000u) >> 8) | ((x & 0xFF000000u) >> 24);
}

Uint64 SDL_Swap64(Uint64 x) {
  x = ((x & 0x00000000FFFFFFFFULL) << 32) | ((x & 0xFFFFFFFF00000000ULL) >> 32);
  x = ((x & 0x0000FFFF000000FFFFULL) << 16) |
      ((x & 0xFFFF0000FFFF0000ULL) >> 16);
  x = ((x & 0x00FF00FF00FF00FFULL) << 8) | ((x & 0xFF00FF00FF00FF00ULL) >> 8);
  return x;
}

void SDL_SetError(const char *fmt, ...) {
  if (!fmt) {
    s_sdl1_error_buf[0] = '\0';
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  SDL_vsnprintf(s_sdl1_error_buf, sizeof(s_sdl1_error_buf), fmt, ap);
  va_end(ap);

  printf("[SDL Error] %s\n", s_sdl1_error_buf);
}

char *SDL_GetError(void) { return s_sdl1_error_buf; }

void SDL_ClearError(void) { s_sdl1_error_buf[0] = '\0'; }

void SDL_Error(SDL_errorcode code) {
  switch (code) {
  case SDL_ENOMEM:
    SDL_SetError("Out of memory");
    break;
  case SDL_EFREAD:
    SDL_SetError("Read error");
    break;
  case SDL_EFWRITE:
    SDL_SetError("Write error");
    break;
  case SDL_EFSEEK:
    SDL_SetError("Seek error");
    break;
  case SDL_UNSUPPORTED:
    SDL_SetError("That operation is not supported");
    break;
  case SDL_LASTERROR:
  default:
    SDL_SetError("Unknown error");
    break;
  }
}