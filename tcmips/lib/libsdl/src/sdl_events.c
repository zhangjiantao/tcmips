/* SDL compatibility layer: events and keyboard.
 *
 * Reads the TCMIPS hardware keyboard directly via tcm_syscall_read_keyboard()
 * (no SDL2 dependency) and feeds a small SDL1 event queue.
 *
 * Upstream SDL-1.2 baseline: release-1.2.15,
 * commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
 */

#include "tcmips_sdl_internal.h"

#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <string.h>

extern void tcmips_sdl_update_timers(void);

/* Map a TCMIPS custom key code to the matching SDL keysym.
 * Printable characters already use their ASCII value (which SDL1 adopts
 * verbatim); the navigation/modifier keys have separate identifiers. */
static SDLKey tcm_keycode_to_sdlkey(uint16_t tcm_code) {
  switch (tcm_code) {
  case TCM_K_UP:
    return SDLK_UP;
  case TCM_K_RIGHT:
    return SDLK_RIGHT;
  case TCM_K_DOWN:
    return SDLK_DOWN;
  case TCM_K_LEFT:
    return SDLK_LEFT;
  case TCM_K_PAGEUP:
    return SDLK_PAGEUP;
  case TCM_K_PAGEDOWN:
    return SDLK_PAGEDOWN;
  case TCM_K_HOME:
    return SDLK_HOME;
  case TCM_K_END:
    return SDLK_END;
  case TCM_K_TAB:
    return SDLK_TAB;
  case TCM_K_ENTER:
    return SDLK_RETURN;
  case TCM_K_INSERT:
    return SDLK_INSERT;
  case TCM_K_DEL:
    return SDLK_DELETE;
  case TCM_K_BACKSPACE:
    return SDLK_BACKSPACE;
  case TCM_K_ESC:
    return SDLK_ESCAPE;
  case TCM_K_LCTRL:
    return SDLK_LCTRL;
  case TCM_K_LSHIFT:
    return SDLK_LSHIFT;
  case TCM_K_LALT:
    return SDLK_LALT;
  case TCM_K_RCTRL:
    return SDLK_RCTRL;
  case TCM_K_RSHIFT:
    return SDLK_RSHIFT;
  case TCM_K_RALT:
    return SDLK_RALT;
  case TCM_K_CAPSLOCK:
    return SDLK_CAPSLOCK;
  default:
    /* Printable ASCII (space to tilde) maps directly to SDL1 keysym. */
    if (tcm_code >= 0x20 && tcm_code <= 0x7E) {
      return (SDLKey)tcm_code;
    }
    return SDLK_UNKNOWN;
  }
}

/* Queue an SDL1 event (tail push, drop on overflow).
 * Returns 1 when the event was actually stored, 0 when the queue was full. */
static int tcmips_sdl_enqueue(const SDL_Event *e) {
  int next = (g_tcmips_sdl.evt_tail + 1) % TCMIPS_SDL_EVENT_QUEUE_SIZE;
  if (next == g_tcmips_sdl.evt_head) {
    return 0; /* queue full, drop */
  }
  g_tcmips_sdl.evt_queue[g_tcmips_sdl.evt_tail] = *e;
  g_tcmips_sdl.evt_tail = next;
  return 1;
}

void SDL_PumpEvents(void) {
  tcmips_sdl_update_timers();

  /* Drain the hardware keyboard, translating each raw packet to SDL1. */
  TcmKeyboardEvent ev;
  for (;;) {
    ev.raw_value = tcm_syscall_read_keyboard();
    if (!ev.event.not_empty) {
      break;
    }

    SDLKey key = tcm_keycode_to_sdlkey((uint16_t)ev.event.key_code);
    if (key == SDLK_UNKNOWN) {
      continue;
    }

    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.key.type = ev.event.is_press ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.which = 0;
    e.key.state = ev.event.is_press ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = key;
    e.key.keysym.mod = SDL_GetModState();
    e.key.keysym.unicode =
        (ev.event.is_press && key >= SDLK_SPACE && key <= SDLK_z) ? (char)key
                                                                  : 0;

    /* Cache the key state, sized for the SDL1 key range. */
    if (key >= 0 && key < SDLK_LAST) {
      g_tcmips_sdl.key_state[(int)key] = ev.event.is_press ? 1 : 0;
    }

    tcmips_sdl_enqueue(&e);
  }
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action,
                   Uint32 mask) {
  if (action == SDL_ADD_EVENT) {
    if (numevents <= 0)
      return 0;
    int added = 0;
    for (int i = 0; i < numevents; ++i) {
      added += tcmips_sdl_enqueue(&events[i]);
    }
    return added; /* report the events actually stored (0 = queue full) */
  }

  if (!events || numevents <= 0)
    return 0;
  if (action != SDL_GETEVENT && action != SDL_PEEKE_EVENT)
    return 0;

  int count = 0;
  int head = g_tcmips_sdl.evt_head;
  while (head != g_tcmips_sdl.evt_tail && count < numevents) {
    if (SDL_EVENTMASK(g_tcmips_sdl.evt_queue[head].type) & mask) {
      events[count] = g_tcmips_sdl.evt_queue[head];
      count++;
    }
    /* SDL_GETEVENT consumes every scanned event (matching ones are
     * returned, non-matching ones are dropped) so the FIFO keeps
     * draining: advance the head unconditionally on every iteration.
     * SDL_PEEKE_EVENT only reads and must not consume any event, so the
     * per-iteration head advance is skipped and evt_head is left intact. */
    if (action == SDL_GETEVENT) {
      head = (head + 1) % TCMIPS_SDL_EVENT_QUEUE_SIZE;
    }
  }
  if (action == SDL_GETEVENT) {
    g_tcmips_sdl.evt_head = head;
  }
  return count;
}

/* Not part of upstream SDL-1.2.15 (SDL_PushEvent arrived with SDL 1.3/2.0);
 * provided as a portability extension layered on SDL_PeepEvents(..., ADD,
 * ...) with the SDL2 contract: 1 = queued, 0 = queue full, -1 = failure. */
int SDL_PushEvent(SDL_Event *event) {
  if (!event)
    return -1;
  return SDL_PeepEvents(event, 1, SDL_ADD_EVENT, 0) == 1 ? 1 : 0;
}

int SDL_PollEvent(SDL_Event *event) {
  SDL_PumpEvents();
  if (g_tcmips_sdl.evt_head == g_tcmips_sdl.evt_tail)
    return 0;
  if (event) {
    *event = g_tcmips_sdl.evt_queue[g_tcmips_sdl.evt_head];
  }
  g_tcmips_sdl.evt_head =
      (g_tcmips_sdl.evt_head + 1) % TCMIPS_SDL_EVENT_QUEUE_SIZE;
  return 1;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (SDL_PollEvent(event) == 0) {
    SDL_Delay(1);
  }
  return 1;
}

Uint8 SDL_EventState(Uint8 type, int state) {
  (void)type;
  (void)state;
  return SDL_ENABLE;
}

Uint8 *SDL_GetKeyState(int *numkeys) {
  if (numkeys)
    *numkeys = SDLK_LAST;
  return g_tcmips_sdl.key_state;
}

/* alias used by some applications written against the SDL 1.2.10+ rename */
Uint8 *SDL_GetKeyboardState(int *numkeys) { return SDL_GetKeyState(numkeys); }

SDLMod SDL_GetModState(void) {
  /* Build a mod mask from cached modifier key state. */
  SDLMod mod = KMOD_NONE;
  if (g_tcmips_sdl.key_state[SDLK_LSHIFT])
    mod = (SDLMod)(mod | KMOD_LSHIFT);
  if (g_tcmips_sdl.key_state[SDLK_RSHIFT])
    mod = (SDLMod)(mod | KMOD_RSHIFT);
  if (g_tcmips_sdl.key_state[SDLK_LCTRL])
    mod = (SDLMod)(mod | KMOD_LCTRL);
  if (g_tcmips_sdl.key_state[SDLK_RCTRL])
    mod = (SDLMod)(mod | KMOD_RCTRL);
  if (g_tcmips_sdl.key_state[SDLK_LALT])
    mod = (SDLMod)(mod | KMOD_LALT);
  if (g_tcmips_sdl.key_state[SDLK_RALT])
    mod = (SDLMod)(mod | KMOD_RALT);
  if (g_tcmips_sdl.key_state[SDLK_CAPSLOCK])
    mod = (SDLMod)(mod | KMOD_CAPS);
  return mod;
}

void SDL_SetModState(SDLMod modstate) { (void)modstate; }

char *SDL_GetKeyName(SDLKey key) {
  static char names[256][8];
  static int inited = 0;
  if (!inited) {
    inited = 1;
    for (int i = 0; i < 256; ++i) {
      names[i][0] = 0;
    }
  }
  if (key >= SDLK_SPACE && key <= SDLK_z) {
    names[key][0] = (char)key;
    names[key][1] = 0;
    return names[key];
  }
  /* Static readable names for a few common keys. */
  switch (key) {
  case SDLK_UP:
    return "up";
  case SDLK_DOWN:
    return "down";
  case SDLK_LEFT:
    return "left";
  case SDLK_RIGHT:
    return "right";
  case SDLK_ESCAPE:
    return "escape";
  case SDLK_RETURN:
    return "return";
  case SDLK_BACKSPACE:
    return "backspace";
  case SDLK_TAB:
    return "tab";
  case SDLK_INSERT:
    return "insert";
  case SDLK_DELETE:
    return "delete";
  case SDLK_HOME:
    return "home";
  case SDLK_END:
    return "end";
  case SDLK_PAGEUP:
    return "pageup";
  case SDLK_PAGEDOWN:
    return "pagedown";
  default:
    return NULL;
  }
}

int SDL_EnableKeyRepeat(int delay, int interval) {
  (void)delay;
  (void)interval;
  return 0;
}

void SDL_GetKeyRepeat(int *delay, int *interval) {
  if (delay)
    *delay = 0;
  if (interval)
    *interval = 0;
}

/* Mouse stubs: TCMIPS has no pointer device, so state stays at origin. */
Uint8 SDL_GetMouseState(int *x, int *y) {
  if (x)
    *x = g_tcmips_sdl.mouse_x;
  if (y)
    *y = g_tcmips_sdl.mouse_y;
  return g_tcmips_sdl.mouse_buttons;
}

Uint8 SDL_GetRelativeMouseState(int *x, int *y) {
  if (x)
    *x = 0;
  if (y)
    *y = 0;
  return 0;
}

void SDL_WarpMouse(Uint16 x, Uint16 y) {
  g_tcmips_sdl.mouse_x = (int)x;
  g_tcmips_sdl.mouse_y = (int)y;
}

/* Cursor stubs */
int SDL_ShowCursor(int toggle) {
  if (toggle == SDL_QUERY)
    return 1;
  return toggle;
}

SDL_Cursor *SDL_CreateCursor(Uint8 *data, Uint8 *mask, int w, int h, int hot_x,
                             int hot_y) {
  (void)data;
  (void)mask;
  (void)w;
  (void)h;
  (void)hot_x;
  (void)hot_y;
  static SDL_Cursor dummy;
  return &dummy;
}

void SDL_SetCursor(SDL_Cursor *cursor) { (void)cursor; }
SDL_Cursor *SDL_GetCursor(void) {
  static SDL_Cursor dummy;
  return &dummy;
}
void SDL_FreeCursor(SDL_Cursor *cursor) { (void)cursor; }