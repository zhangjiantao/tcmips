/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_events.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_events_h
#define _SDL_events_h

#include "SDL_types.h"
#include "SDL_video.h"
#include "SDL_keyboard.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Event types -- mapped to SDL numbers */
typedef enum {
    SDL_NOEVENT = 0,
    SDL_ACTIVEEVENT,
    SDL_KEYDOWN,
    SDL_KEYUP,
    SDL_MOUSEMOTION,
    SDL_MOUSEBUTTONDOWN,
    SDL_MOUSEBUTTONUP,
    SDL_JOYAXISMOTION,
    SDL_JOYBALLMOTION,
    SDL_JOYHATMOTION,
    SDL_JOYBUTTONDOWN,
    SDL_JOYBUTTONUP,
    SDL_QUIT,
    SDL_SYSWMEVENT,
    SDL_EVENT_RESERVEDA,
    SDL_EVENT_RESERVEDB,
    SDL_VIDEORESIZE,
    SDL_VIDEOEXPOSE,
    SDL_EVENT_RESERVED2,
    SDL_EVENT_RESERVED3,
    SDL_EVENT_RESERVED4,
    SDL_EVENT_RESERVED5,
    SDL_EVENT_RESERVED6,
    SDL_EVENT_RESERVED7,
    SDL_USEREVENT = 24,
    SDL_NUMEVENTS = 32
} SDL_EventType;

#define SDL_ACTIVEEVENTMASK	    SDL_EVENTMASK(SDL_ACTIVEEVENT)
#define SDL_KEYDOWNMASK		    SDL_EVENTMASK(SDL_KEYDOWN)
#define SDL_KEYUPMASK		    SDL_EVENTMASK(SDL_KEYUP)
#define SDL_KEYEVENTMASK	    (SDL_KEYDOWNMASK | SDL_KEYUPMASK)
#define SDL_MOUSEMOTIONMASK	    SDL_EVENTMASK(SDL_MOUSEMOTION)
#define SDL_MOUSEBUTTONDOWNMASK	SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN)
#define SDL_MOUSEBUTTONUPMASK	SDL_EVENTMASK(SDL_MOUSEBUTTONUP)
#define SDL_MOUSEEVENTMASK	    (SDL_MOUSEMOTIONMASK | SDL_MOUSEBUTTONDOWNMASK | SDL_MOUSEBUTTONUPMASK)
#define SDL_JOYAXISMOTIONMASK	SDL_EVENTMASK(SDL_JOYAXISMOTION)
#define SDL_JOYBALLMOTIONMASK	SDL_EVENTMASK(SDL_JOYBALLMOTION)
#define SDL_JOYHATMOTIONMASK	SDL_EVENTMASK(SDL_JOYHATMOTION)
#define SDL_JOYBUTTONDOWNMASK	SDL_EVENTMASK(SDL_JOYBUTTONDOWN)
#define SDL_JOYBUTTONUPMASK	    SDL_EVENTMASK(SDL_JOYBUTTONUP)
#define SDL_JOYEVENTMASK	    (SDL_JOYAXISMOTIONMASK | SDL_JOYBALLMOTIONMASK | SDL_JOYHATMOTIONMASK | SDL_JOYBUTTONDOWNMASK | SDL_JOYBUTTONUPMASK)
#define SDL_VIDEORESIZEMASK	    SDL_EVENTMASK(SDL_VIDEORESIZE)
#define SDL_VIDEOEXPOSEMASK	    SDL_EVENTMASK(SDL_VIDEOEXPOSE)
#define SDL_QUITMASK		    SDL_EVENTMASK(SDL_QUIT)
#define SDL_SYSWMEVENTMASK	    SDL_EVENTMASK(SDL_SYSWMEVENT)

#define SDL_EVENTMASK(X)	(1<<(X))

typedef enum {
    SDL_RELEASED = 0,
    SDL_PRESSED = 1
} SDL_ButtonState;

typedef struct SDL_ActiveEvent {
    Uint8 type;
    Uint8 gain;
    Uint8 state;
} SDL_ActiveEvent;

typedef struct SDL_KeyboardEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    SDL_keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    Uint16 x, y;
    Sint16 xrel, yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    Uint8 type;
    Uint8 which;
    Uint8 button;
    Uint8 state;
    Uint16 x, y;
} SDL_MouseButtonEvent;

typedef struct SDL_JoyAxisEvent {
    Uint8 type;
    Uint8 which;
    Uint8 axis;
    Sint16 value;
} SDL_JoyAxisEvent;

typedef struct SDL_JoyBallEvent {
    Uint8 type;
    Uint8 which;
    Uint8 ball;
    Sint16 xrel, yrel;
} SDL_JoyBallEvent;

typedef struct SDL_JoyHatEvent {
    Uint8 type;
    Uint8 which;
    Uint8 hat;
    Uint8 value;
} SDL_JoyHatEvent;

typedef struct SDL_JoyButtonEvent {
    Uint8 type;
    Uint8 which;
    Uint8 button;
    Uint8 state;
} SDL_JoyButtonEvent;

typedef struct SDL_ResizeEvent {
    Uint8 type;
    int w;
    int h;
} SDL_ResizeEvent;

typedef struct SDL_ExposeEvent {
    Uint8 type;
} SDL_ExposeEvent;

typedef struct SDL_QuitEvent {
    Uint8 type;
} SDL_QuitEvent;

typedef struct SDL_UserEvent {
    Uint8 type;
    int code;
    void *data1;
    void *data2;
} SDL_UserEvent;

typedef struct SDL_SysWMEvent {
    Uint8 type;
    struct SDL_SysWMmsg *msg;
} SDL_SysWMEvent;

typedef union SDL_Event {
    Uint8 type;
    SDL_ActiveEvent active;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyBallEvent jball;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_ResizeEvent resize;
    SDL_ExposeEvent expose;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
    SDL_SysWMEvent syswm;
} SDL_Event;

/* Event queue manipulation */
typedef enum {
    SDL_ADD_EVENT,
    SDL_PEEKE_EVENT,
    SDL_GETEVENT
} SDL_eventaction;

/* Upstream SDL-1.2 spells these without the extra underscore; provide the
 * upstream names as macros so existing SDL 1.2 code that calls
 * SDL_PeepEvents(..., SDL_ADDEVENT / SDL_PEEKEVENT, ...) compiles here. */
#define SDL_ADDEVENT    SDL_ADD_EVENT
#define SDL_PEEKEVENT   SDL_PEEKE_EVENT

SDL_DECLSPEC void SDLCALL SDL_PumpEvents(void);
SDL_DECLSPEC int SDLCALL SDL_PeepEvents(SDL_Event *events, int numevents,
                                         SDL_eventaction action, Uint32 mask);
/* Not in upstream SDL-1.2.15: SDL_PushEvent arrived with SDL 1.3/2.0.
 * Provided as a portability extension with the SDL2 contract (1 queued,
 * 0 queue full, -1 failure), layered on SDL_PeepEvents(..., SDL_ADDEVENT,
 * ...). */
SDL_DECLSPEC int SDLCALL SDL_PushEvent(SDL_Event *event);
SDL_DECLSPEC int SDLCALL SDL_PollEvent(SDL_Event *event);
SDL_DECLSPEC int SDLCALL SDL_WaitEvent(SDL_Event *event);
SDL_DECLSPEC Uint8 SDLCALL SDL_EventState(Uint8 type, int state);
#define SDL_QUERY   -1
#define SDL_ENABLE   1
#define SDL_IGNORE   0

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_events_h */
