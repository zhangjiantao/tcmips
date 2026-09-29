#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <string.h>

#define TCMIPS_SDL_HINT_MAX 64

typedef struct {
  const char *name;
  char value[128];
} tcmips_sdl2_hint_t;

static tcmips_sdl2_hint_t s_hints[TCMIPS_SDL_HINT_MAX];

static tcmips_sdl2_hint_t *tcmips_sdl2_find_hint(const char *name) {
  for (int i = 0; i < TCMIPS_SDL_HINT_MAX; ++i) {
    if (s_hints[i].name && SDL_strcmp(s_hints[i].name, name) == 0) {
      return &s_hints[i];
    }
  }
  return NULL;
}

static tcmips_sdl2_hint_t *tcmips_sdl2_alloc_hint(const char *name) {
  tcmips_sdl2_hint_t *h = tcmips_sdl2_find_hint(name);
  if (h)
    return h;
  for (int i = 0; i < TCMIPS_SDL_HINT_MAX; ++i) {
    if (s_hints[i].name == NULL) {
      s_hints[i].name = name;
      s_hints[i].value[0] = '\0';
      return &s_hints[i];
    }
  }
  return NULL;
}

SDL_bool SDL_SetHint(const char *name, const char *value) {
  return SDL_SetHintWithPriority(name, value, SDL_HINT_NORMAL);
}

SDL_bool SDL_SetHintWithPriority(const char *name, const char *value,
                                 SDL_HintPriority priority) {
  (void)priority;
  if (!name || !value)
    return SDL_FALSE;
  tcmips_sdl2_hint_t *h = tcmips_sdl2_alloc_hint(name);
  if (!h)
    return SDL_FALSE;
  SDL_strlcpy(h->value, value, sizeof(h->value));
  return SDL_TRUE;
}

const char *SDL_GetHint(const char *name) {
  tcmips_sdl2_hint_t *h = tcmips_sdl2_find_hint(name);
  if (h)
    return h->value;
  return NULL;
}

SDL_bool SDL_GetHintBoolean(const char *name, SDL_bool default_value) {
  const char *v = SDL_GetHint(name);
  if (!v)
    return default_value;
  if (SDL_strcmp(v, "1") == 0 || SDL_strcasecmp(v, "true") == 0 ||
      SDL_strcasecmp(v, "yes") == 0 || SDL_strcasecmp(v, "on") == 0) {
    return SDL_TRUE;
  }
  if (SDL_strcmp(v, "0") == 0 || SDL_strcasecmp(v, "false") == 0 ||
      SDL_strcasecmp(v, "no") == 0 || SDL_strcasecmp(v, "off") == 0) {
    return SDL_FALSE;
  }
  return default_value;
}

void SDL_ClearHints(void) {
  for (int i = 0; i < TCMIPS_SDL_HINT_MAX; ++i) {
    s_hints[i].name = NULL;
    s_hints[i].value[0] = '\0';
  }
}

void SDL_AddHintCallback(const char *name, SDL_HintCallback callback,
                         void *userdata) {
  (void)name;
  (void)callback;
  (void)userdata;
  TCMIPS_SDL_STUB_MSG("SDL_AddHintCallback");
}

void SDL_DelHintCallback(const char *name, SDL_HintCallback callback,
                         void *userdata) {
  (void)name;
  (void)callback;
  (void)userdata;
  TCMIPS_SDL_STUB_MSG("SDL_DelHintCallback");
}
