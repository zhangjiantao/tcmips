#include <SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char s_error_buf[256];

int SDL_SetError(const char *fmt, ...) {
  if (!fmt) {
    s_error_buf[0] = '\0';
    return -1;
  }
  va_list ap;
  va_start(ap, fmt);
  SDL_vsnprintf(s_error_buf, sizeof(s_error_buf), fmt, ap);
  va_end(ap);

  printf("[SDL Error] %s\n", s_error_buf);
  return -1;
}

const char *SDL_GetError(void) { return s_error_buf; }

void SDL_ClearError(void) { s_error_buf[0] = '\0'; }

int SDL_Error(SDL_errorcode code) {
  switch (code) {
  case SDL_ENOMEM:
    SDL_SetError("Out of memory");
    return -1;
  case SDL_EFREAD:
    SDL_SetError("Read error");
    return -1;
  case SDL_EFWRITE:
    SDL_SetError("Write error");
    return -1;
  case SDL_EFSEEK:
    SDL_SetError("Seek error");
    return -1;
  case SDL_UNSUPPORTED:
    SDL_SetError("That operation is not supported");
    return -1;
  case SDL_LASTERROR:
  default:
    SDL_SetError("Unknown error");
    return -1;
  }
}

char *SDL_GetErrorMsg(char *errstr, int maxlen) {
  if (errstr && maxlen > 0) {
    SDL_strlcpy(errstr, s_error_buf, (size_t)maxlen);
  }
  return errstr;
}
