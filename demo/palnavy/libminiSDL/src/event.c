#include <NDL.h>
#include <SDL.h>

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include "util.h"

#define keyname(k) #k,
static const char *keyname[] = {"NONE", _KEYS(keyname)};

static uint8_t tcm_key_state[512] = {0};

int SDL_PushEvent(SDL_Event *ev) { return 0; }

int SDL_PollEvent(SDL_Event *ev) {
  if (!ev)
    return 0;

  uint32_t raw_key_data = tcm_syscall_read_keyboard();
  if (__TCM_KEYBOARD_EMPTY(raw_key_data)) {
    return 0;
  }

  int is_press = __TCM_KEYBOARD_PRESS(raw_key_data);
  uint32_t kcode = __TCM_KEYBOARD_KCODE(raw_key_data);
  int sdl_sym = 0;

  // UTIL_LogOutput(LOGLEVEL_INFO, "key %x press %s\n", kcode, is_press ?
  // "pressed" : "released");

  switch (kcode) {
  case __TCM_KEY_CODE_UP:
    sdl_sym = SDLK_UP;
    break;
  case __TCM_KEY_CODE_DOWN:
    sdl_sym = SDLK_DOWN;
    break;
  case __TCM_KEY_CODE_LEFT:
    sdl_sym = SDLK_LEFT;
    break;
  case __TCM_KEY_CODE_RIGHT:
    sdl_sym = SDLK_RIGHT;
    break;
  case __TCM_KEY_CODE_ENTER:
    sdl_sym = SDLK_RETURN;
    break;
  case __TCM_KEY_CODE_TAB:
    sdl_sym = SDLK_ESCAPE;
    break;
  case __TCM_KEY_CODE_BACKSPACE:
    sdl_sym = SDLK_ESCAPE;
    break;
  case __TCM_KEY_CODE_DEL:
    sdl_sym = SDLK_ESCAPE;
    break;
  case __TCM_KEY_CODE_LSHIFT:
    sdl_sym = SDLK_LSHIFT;
    break;
  case __TCM_KEY_CODE_RSHIFT:
    sdl_sym = SDLK_RSHIFT;
    break;
  default:
    return 0;
  }

  if (sdl_sym != 0) {
    tcm_key_state[sdl_sym] = is_press ? 1 : 0;
  }

  ev->type = is_press ? SDL_KEYDOWN : SDL_KEYUP;
  ev->key.keysym.sym = sdl_sym;

  return 1;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (!SDL_PollEvent(event))
    ;
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t *SDL_GetKeyState(int *numkeys) {
  if (numkeys) {
    *numkeys = 512;
  }
  return tcm_key_state;
}