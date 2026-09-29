/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_joystick.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_joystick_h
#define _SDL_joystick_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* The joystick structure is opaque. */
typedef struct SDL_Joystick SDL_Joystick;

SDL_DECLSPEC int SDLCALL SDL_NumJoysticks(void);
SDL_DECLSPEC const char * SDLCALL SDL_JoystickName(int device_index);
SDL_DECLSPEC SDL_Joystick * SDLCALL SDL_JoystickOpen(int device_index);
SDL_DECLSPEC void SDLCALL SDL_JoystickClose(SDL_Joystick *joystick);
SDL_DECLSPEC int SDLCALL SDL_JoystickOpened(int device_index);
SDL_DECLSPEC int SDLCALL SDL_JoystickIndex(SDL_Joystick *joystick);
SDL_DECLSPEC int SDLCALL SDL_JoystickNumAxes(SDL_Joystick *joystick);
SDL_DECLSPEC int SDLCALL SDL_JoystickNumBalls(SDL_Joystick *joystick);
SDL_DECLSPEC int SDLCALL SDL_JoystickNumHats(SDL_Joystick *joystick);
SDL_DECLSPEC int SDLCALL SDL_JoystickNumButtons(SDL_Joystick *joystick);
SDL_DECLSPEC void SDLCALL SDL_JoystickUpdate(void);
SDL_DECLSPEC int SDLCALL SDL_JoystickEventState(int state);
SDL_DECLSPEC Sint16 SDLCALL SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis);
SDL_DECLSPEC Uint8 SDLCALL SDL_JoystickGetHat(SDL_Joystick *joystick, int hat);
SDL_DECLSPEC int SDLCALL SDL_JoystickGetBall(SDL_Joystick *joystick, int ball, int *dx, int *dy);
SDL_DECLSPEC Uint8 SDLCALL SDL_JoystickGetButton(SDL_Joystick *joystick, int button);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_joystick_h */
