#include <SDL.h>

#include <stdlib.h>
#include <string.h>

char *SDL_GetBasePath(void) {
  char *p = SDL_malloc(2);
  if (!p)
    return NULL;
  p[0] = '/';
  p[1] = '\0';
  return p;
}

char *SDL_GetPrefPath(const char *org, const char *app) {
  (void)org;
  (void)app;
  // TCMIPS runs on a memory filesystem; return a fixed per-user prefix.
  const char *path = "/pref";
  char *p = SDL_malloc(SDL_strlen(path) + 1);
  if (!p)
    return NULL;
  strcpy(p, path);
  return p;
}

char *SDL_GetClipboardText(void) { return SDL_strdup(""); }

int SDL_SetClipboardText(const char *text) {
  (void)text;
  return 0;
}

SDL_bool SDL_HasClipboardText(void) { return SDL_FALSE; }
