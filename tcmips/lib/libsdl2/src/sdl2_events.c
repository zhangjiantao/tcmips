#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <string.h>

// TCMIPS key code -> SDL scancode mapping.
static SDL_Scancode tcm_keycode_to_scancode(uint16_t tcm_code) {
  switch (tcm_code) {
  case TCM_K_UP:
    return SDL_SCANCODE_UP;
  case TCM_K_DOWN:
    return SDL_SCANCODE_DOWN;
  case TCM_K_LEFT:
    return SDL_SCANCODE_LEFT;
  case TCM_K_RIGHT:
    return SDL_SCANCODE_RIGHT;
  case TCM_K_PAGEUP:
    return SDL_SCANCODE_PAGEUP;
  case TCM_K_PAGEDOWN:
    return SDL_SCANCODE_PAGEDOWN;
  case TCM_K_HOME:
    return SDL_SCANCODE_HOME;
  case TCM_K_END:
    return SDL_SCANCODE_END;
  case TCM_K_TAB:
    return SDL_SCANCODE_TAB;
  case TCM_K_ENTER:
    return SDL_SCANCODE_RETURN;
  case TCM_K_INSERT:
    return SDL_SCANCODE_INSERT;
  case TCM_K_DEL:
    return SDL_SCANCODE_DELETE;
  case TCM_K_BACKSPACE:
    return SDL_SCANCODE_BACKSPACE;
  case TCM_K_ESC:
    return SDL_SCANCODE_ESCAPE;
  case TCM_K_LCTRL:
    return SDL_SCANCODE_LCTRL;
  case TCM_K_LSHIFT:
    return SDL_SCANCODE_LSHIFT;
  case TCM_K_LALT:
    return SDL_SCANCODE_LALT;
  case TCM_K_RCTRL:
    return SDL_SCANCODE_RCTRL;
  case TCM_K_RSHIFT:
    return SDL_SCANCODE_RSHIFT;
  case TCM_K_RALT:
    return SDL_SCANCODE_RALT;
  case TCM_K_CAPSLOCK:
    return SDL_SCANCODE_CAPSLOCK;
  default:
    if (tcm_code >= 'a' && tcm_code <= 'z')
      return SDL_SCANCODE_A + (tcm_code - 'a');
    if (tcm_code == '0')
      return SDL_SCANCODE_0;
    if (tcm_code >= '1' && tcm_code <= '9')
      return SDL_SCANCODE_1 + (tcm_code - '1');
    if (tcm_code == ' ')
      return SDL_SCANCODE_SPACE;
    if (tcm_code == ',')
      return SDL_SCANCODE_COMMA;
    if (tcm_code == '.')
      return SDL_SCANCODE_PERIOD;
    if (tcm_code == '/')
      return SDL_SCANCODE_SLASH;
    if (tcm_code == ';')
      return SDL_SCANCODE_SEMICOLON;
    if (tcm_code == '-')
      return SDL_SCANCODE_MINUS;
    if (tcm_code == '=')
      return SDL_SCANCODE_EQUALS;
    if (tcm_code == '[')
      return SDL_SCANCODE_LEFTBRACKET;
    if (tcm_code == ']')
      return SDL_SCANCODE_RIGHTBRACKET;
    if (tcm_code == '\'')
      return SDL_SCANCODE_APOSTROPHE;
    if (tcm_code == '\\')
      return SDL_SCANCODE_BACKSLASH;
    if (tcm_code == '`')
      return SDL_SCANCODE_GRAVE;
    if (tcm_code >= 1 && tcm_code <= 26)
      return SDL_SCANCODE_A + (tcm_code - 1); // Ctrl-A..Z
    return SDL_SCANCODE_UNKNOWN;
  }
}

/* Push an event onto the FIFO (tail push, drop on overflow).
 * Returns SDL_TRUE when the event was actually stored, SDL_FALSE when the
 * queue was full (mirrors the SDL1 layer and the SDL2 SDL_PushEvent contract).
 */
SDL_bool tcmips_sdl2_post_event(const SDL_Event *e) {
  uint32_t next =
      (g_tcmips_sdl.evt_head + 1) & (TCMIPS_SDL_EVENT_QUEUE_SIZE - 1);
  if (next == g_tcmips_sdl.evt_tail) {
    return SDL_FALSE; // queue full, drop
  }
  g_tcmips_sdl.evt_queue[g_tcmips_sdl.evt_head] = *e;
  g_tcmips_sdl.evt_head = next;
  return SDL_TRUE;
}

void tcmips_sdl2_poll_hardware(void) {
  TcmKeyboardEvent ev;
  for (;;) {
    ev.raw_value = tcm_syscall_read_keyboard();
    if (!ev.event.not_empty) {
      break;
    }

    SDL_Scancode sc = tcm_keycode_to_scancode((uint16_t)ev.event.key_code);
    if (sc == SDL_SCANCODE_UNKNOWN) {
      continue;
    }

    SDL_Event e;
    SDL_zero(e);
    e.type = ev.event.is_press ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.timestamp = SDL_GetTicks();
    e.key.windowID = g_tcmips_sdl.win.window ? g_tcmips_sdl.win.window->id : 0;
    e.key.state = ev.event.is_press ? SDL_PRESSED : SDL_RELEASED;
    e.key.repeat = 0;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.keysym.mod = KMOD_NONE;

    g_tcmips_sdl.key_state[sc] = ev.event.is_press ? 1 : 0;
    tcmips_sdl2_post_event(&e);
  }
}

int SDL_PollEvent(SDL_Event *event) {
  tcmips_sdl2_poll_hardware();

  if (g_tcmips_sdl.evt_head == g_tcmips_sdl.evt_tail) {
    return 0;
  }

  if (event) {
    *event = g_tcmips_sdl.evt_queue[g_tcmips_sdl.evt_tail];
  }
  g_tcmips_sdl.evt_tail =
      (g_tcmips_sdl.evt_tail + 1) & (TCMIPS_SDL_EVENT_QUEUE_SIZE - 1);
  return 1;
}

int SDL_WaitEvent(SDL_Event *event) {
  while (SDL_PollEvent(event) == 0) {
    // Busy-wait until a key event arrives.
  }
  return 1;
}

int SDL_WaitEventTimeout(SDL_Event *event, int timeout) {
  Uint32 start = SDL_GetTicks();
  int result;
  while ((result = SDL_PollEvent(event)) == 0) {
    if ((SDL_GetTicks() - start) >= (Uint32)timeout) {
      return 0;
    }
  }
  return result;
}

int SDL_PushEvent(SDL_Event *event) {
  if (!event) {
    return 0;
  }
  return tcmips_sdl2_post_event(event) ? 1 : 0;
}

void SDL_PumpEvents(void) { tcmips_sdl2_poll_hardware(); }

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action,
                   Uint32 minType, Uint32 maxType) {
  if (action == SDL_PEEKEVENT) {
    int count = 0;
    uint32_t i = g_tcmips_sdl.evt_tail;
    while (i != g_tcmips_sdl.evt_head && count < numevents) {
      events[count++] = g_tcmips_sdl.evt_queue[i];
      i = (i + 1) & (TCMIPS_SDL_EVENT_QUEUE_SIZE - 1);
    }
    return count;
  } else if (action == SDL_GETEVENT) {
    /* SDL2 semantics: remove and return matching events, keep the
     * unmatched ones in the FIFO (in order).  Rebuilding from a snapshot
     * is simple and correct for this small queue. */
    SDL_Event snapshot[TCMIPS_SDL_EVENT_QUEUE_SIZE];
    int nsnap = 0;
    uint32_t h = g_tcmips_sdl.evt_head;
    uint32_t t = g_tcmips_sdl.evt_tail;
    while (t != h) {
      snapshot[nsnap++] = g_tcmips_sdl.evt_queue[t];
      t = (t + 1) & (TCMIPS_SDL_EVENT_QUEUE_SIZE - 1);
    }
    g_tcmips_sdl.evt_head = 0;
    g_tcmips_sdl.evt_tail = 0;
    int count = 0;
    for (int i = 0; i < nsnap; ++i) {
      Uint32 ty = snapshot[i].type;
      if (ty >= minType && ty <= maxType && count < numevents) {
        events[count++] = snapshot[i];
      } else {
        /* keep unmatched / not-yet-copied events in FIFO order */
        g_tcmips_sdl.evt_queue[g_tcmips_sdl.evt_head] = snapshot[i];
        g_tcmips_sdl.evt_head =
            (g_tcmips_sdl.evt_head + 1) & (TCMIPS_SDL_EVENT_QUEUE_SIZE - 1);
      }
    }
    return count;
  } else if (action == SDL_ADDEVENT) {
    int added = 0;
    for (int i = 0; i < numevents; ++i) {
      if (SDL_PushEvent(&events[i]) == 1) {
        ++added;
      }
    }
    return added; /* report events actually stored (0 = queue full) */
  }
  return -1;
}

const Uint8 *SDL_GetKeyboardState(int *numkeys) {
  if (numkeys) {
    *numkeys = SDL_NUM_SCANCODES;
  }
  return g_tcmips_sdl.key_state;
}

SDL_Keymod SDL_GetModState(void) { return KMOD_NONE; }

void SDL_SetModState(SDL_Keymod modstate) { (void)modstate; }

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode) {
  // SDL provides a default mapping built into the library.
  // For TCMIPS we only need the subset that corresponds to ASCII
  // letters/digits.
  if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
    return SDLK_a + (scancode - SDL_SCANCODE_A);
  }
  if (scancode == SDL_SCANCODE_0)
    return SDLK_0;
  if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
    return SDLK_1 + (scancode - SDL_SCANCODE_1);
  }
  switch (scancode) {
  case SDL_SCANCODE_SPACE:
    return SDLK_SPACE;
  case SDL_SCANCODE_RETURN:
    return SDLK_RETURN;
  case SDL_SCANCODE_ESCAPE:
    return SDLK_ESCAPE;
  case SDL_SCANCODE_BACKSPACE:
    return SDLK_BACKSPACE;
  case SDL_SCANCODE_TAB:
    return SDLK_TAB;
  case SDL_SCANCODE_UP:
    return SDLK_UP;
  case SDL_SCANCODE_DOWN:
    return SDLK_DOWN;
  case SDL_SCANCODE_LEFT:
    return SDLK_LEFT;
  case SDL_SCANCODE_RIGHT:
    return SDLK_RIGHT;
  case SDL_SCANCODE_HOME:
    return SDLK_HOME;
  case SDL_SCANCODE_END:
    return SDLK_END;
  case SDL_SCANCODE_PAGEUP:
    return SDLK_PAGEUP;
  case SDL_SCANCODE_PAGEDOWN:
    return SDLK_PAGEDOWN;
  case SDL_SCANCODE_DELETE:
    return SDLK_DELETE;
  case SDL_SCANCODE_INSERT:
    return SDLK_INSERT;
  case SDL_SCANCODE_LSHIFT:
  case SDL_SCANCODE_RSHIFT:
    return SDLK_LSHIFT;
  case SDL_SCANCODE_LCTRL:
  case SDL_SCANCODE_RCTRL:
    return SDLK_LCTRL;
  case SDL_SCANCODE_LALT:
  case SDL_SCANCODE_RALT:
    return SDLK_LALT;
  default:
    return SDLK_UNKNOWN;
  }
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key) {
  if (key >= SDLK_a && key <= SDLK_z) {
    return (SDL_Scancode)(SDL_SCANCODE_A + (key - SDLK_a));
  }
  if (key >= SDLK_0 && key <= SDLK_9) {
    return (SDL_Scancode)(SDL_SCANCODE_0 + (key - SDLK_0));
  }
  switch (key) {
  case SDLK_SPACE:
    return SDL_SCANCODE_SPACE;
  case SDLK_RETURN:
    return SDL_SCANCODE_RETURN;
  case SDLK_ESCAPE:
    return SDL_SCANCODE_ESCAPE;
  case SDLK_BACKSPACE:
    return SDL_SCANCODE_BACKSPACE;
  case SDLK_TAB:
    return SDL_SCANCODE_TAB;
  case SDLK_UP:
    return SDL_SCANCODE_UP;
  case SDLK_DOWN:
    return SDL_SCANCODE_DOWN;
  case SDLK_LEFT:
    return SDL_SCANCODE_LEFT;
  case SDLK_RIGHT:
    return SDL_SCANCODE_RIGHT;
  case SDLK_HOME:
    return SDL_SCANCODE_HOME;
  case SDLK_END:
    return SDL_SCANCODE_END;
  case SDLK_PAGEUP:
    return SDL_SCANCODE_PAGEUP;
  case SDLK_PAGEDOWN:
    return SDL_SCANCODE_PAGEDOWN;
  case SDLK_DELETE:
    return SDL_SCANCODE_DELETE;
  case SDLK_INSERT:
    return SDL_SCANCODE_INSERT;
  default:
    break;
  }
  return SDL_SCANCODE_UNKNOWN;
}

const char *SDL_GetScancodeName(SDL_Scancode scancode) {
  // SDL2 provides a comprehensive name table; this is a minimal fallback.
  static char name_buf[32];
  if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
    static const char *names[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                  "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                  "S", "T", "U", "V", "W", "X", "Y", "Z"};
    return names[scancode - SDL_SCANCODE_A];
  }
  if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
    static const char *names[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    return names[scancode - SDL_SCANCODE_1];
  }
  if (scancode == SDL_SCANCODE_0)
    return "0";
  switch (scancode) {
  case SDL_SCANCODE_UP:
    return "Up";
  case SDL_SCANCODE_DOWN:
    return "Down";
  case SDL_SCANCODE_LEFT:
    return "Left";
  case SDL_SCANCODE_RIGHT:
    return "Right";
  case SDL_SCANCODE_RETURN:
    return "Return";
  case SDL_SCANCODE_ESCAPE:
    return "Escape";
  case SDL_SCANCODE_BACKSPACE:
    return "Backspace";
  case SDL_SCANCODE_TAB:
    return "Tab";
  case SDL_SCANCODE_SPACE:
    return "Space";
  default:
    break;
  }
  name_buf[0] = '[';
  name_buf[1] = (char)('0' + (scancode / 100));
  name_buf[2] = (char)('0' + ((scancode / 10) % 10));
  name_buf[3] = (char)('0' + (scancode % 10));
  name_buf[4] = ']';
  name_buf[5] = '\0';
  return name_buf;
}

SDL_Scancode SDL_GetScancodeFromName(const char *name) {
  if (!name)
    return SDL_SCANCODE_UNKNOWN;
  for (int i = SDL_SCANCODE_UNKNOWN + 1; i < SDL_NUM_SCANCODES; ++i) {
    const char *n = SDL_GetScancodeName((SDL_Scancode)i);
    if (n && SDL_strcasecmp(n, name) == 0) {
      return (SDL_Scancode)i;
    }
  }
  return SDL_SCANCODE_UNKNOWN;
}

const char *SDL_GetKeyName(SDL_Keycode key) {
  return SDL_GetScancodeName(SDL_GetScancodeFromKey(key));
}

SDL_Keycode SDL_GetKeyFromName(const char *name) {
  if (!name)
    return SDLK_UNKNOWN;
  return SDL_GetKeyFromScancode(SDL_GetScancodeFromName(name));
}

// Text input stubs: TCMIPS only has a raw key console, so text events are not
// generated.
void SDL_StartTextInput(void) {}
void SDL_StopTextInput(void) {}
SDL_bool SDL_IsTextInputActive(void) { return SDL_FALSE; }
void SDL_SetTextInputRect(const SDL_Rect *rect) { (void)rect; }
void SDL_ClearComposition(void) {}

SDL_bool SDL_HasScreenKeyboardSupport(void) { return SDL_FALSE; }
SDL_bool SDL_IsScreenKeyboardShown(SDL_Window *window) {
  (void)window;
  return SDL_FALSE;
}

// Misc event helpers (not very useful without real associated hardware).
void SDL_SetEventFilter(SDL_EventFilter filter, void *userdata) {
  (void)filter;
  (void)userdata;
}

SDL_bool SDL_GetEventFilter(SDL_EventFilter *filter, void **userdata) {
  (void)filter;
  (void)userdata;
  return SDL_FALSE;
}

void SDL_AddEventWatch(SDL_EventFilter filter, void *userdata) {
  (void)filter;
  (void)userdata;
}

void SDL_DelEventWatch(SDL_EventFilter filter, void *userdata) {
  (void)filter;
  (void)userdata;
}

void SDL_FilterEvents(SDL_EventFilter filter, void *userdata) {
  (void)filter;
  (void)userdata;
}

Uint8 SDL_EventState(Uint32 type, int state) {
  (void)type;
  (void)state;
  return SDL_ENABLE;
}

SDL_bool SDL_SetEventEnabled(Uint32 type, SDL_bool state) {
  (void)type;
  return state;
}

void SDL_FlushEvent(Uint32 type) { (void)type; }

void SDL_FlushEvents(Uint32 minType, Uint32 maxType) {
  (void)minType;
  (void)maxType;
}

SDL_bool SDL_HasEvent(Uint32 type) {
  (void)type;
  return SDL_FALSE;
}

SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType) {
  (void)minType;
  (void)maxType;
  return SDL_FALSE;
}

// Dollar gesture API stubs.
int SDL_RecordGesture(SDL_TouchID touchId) {
  (void)touchId;
  TCMIPS_SDL_STUB_MSG("SDL_RecordGesture");
  return 0;
}

int SDL_SaveAllDollarTemplates(SDL_RWops *dst) {
  (void)dst;
  TCMIPS_SDL_STUB_MSG("SDL_SaveAllDollarTemplates");
  return 0;
}

int SDL_SaveDollarTemplate(SDL_GestureID gestureId, SDL_RWops *dst) {
  (void)gestureId;
  (void)dst;
  TCMIPS_SDL_STUB_MSG("SDL_SaveDollarTemplate");
  return 0;
}

int SDL_LoadDollarTemplates(SDL_TouchID touchId, SDL_RWops *src) {
  (void)touchId;
  (void)src;
  TCMIPS_SDL_STUB_MSG("SDL_LoadDollarTemplates");
  return 0;
}
