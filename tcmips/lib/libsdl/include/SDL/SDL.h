/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_H
#define _SDL_H

#include "SDL_types.h"
#include "SDL_stdinc.h"
#include "SDL_endian.h"
#include "SDL_video.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "SDL_rwops.h"
#include "SDL_error.h"
#include "SDL_joystick.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_compat.h"

/* This is the version number macro for the current SDL version */
#define SDL_MAJOR_VERSION	1
#define SDL_MINOR_VERSION	2
#define SDL_PATCHLEVEL		15

/* This macro can be used to fill a version structure with the compile-time
 * version of the SDL library.
 */
#define SDL_VERSION(X)                                                  \
{                                                                       \
    (X)->major = SDL_MAJOR_VERSION;                                     \
    (X)->minor = SDL_MINOR_VERSION;                                     \
    (X)->patch = SDL_PATCHLEVEL;                                        \
}

/* This function gets the version of the dynamically linked SDL library. */
typedef struct {
    Uint8 major;
    Uint8 minor;
    Uint8 patch;
} SDL_version;

SDL_DECLSPEC const SDL_version * SDLCALL SDL_Linked_Version(void);

/* SDL_INIT_* flags */
#define SDL_INIT_TIMER          0x00000001
#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_VIDEO          0x00000020
#define SDL_INIT_CDROM          0x00000100
#define SDL_INIT_JOYSTICK       0x00000200
#define SDL_INIT_NOPARACHUTE    0x00100000
#define SDL_INIT_EVENTTHREAD    0x01000000
#define SDL_INIT_EVERYTHING     0x0000FFFF

SDL_DECLSPEC int SDLCALL SDL_Init(Uint32 flags);
SDL_DECLSPEC int SDLCALL SDL_InitSubSystem(Uint32 flags);
SDL_DECLSPEC void SDLCALL SDL_QuitSubSystem(Uint32 flags);
SDL_DECLSPEC Uint32 SDLCALL SDL_WasInit(Uint32 flags);
SDL_DECLSPEC void SDLCALL SDL_Quit(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_H */
