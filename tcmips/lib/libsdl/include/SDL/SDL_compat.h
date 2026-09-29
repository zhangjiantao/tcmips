/*
  Simple DirectMedia Layer 1.2 compatibility extension for TCMIPS.
  Pulled in automatically from SDL.h. Declares SDL symbols whose prototypes
  are not present in any per-subsystem header (CD-ROM, threads, mutexes,
  semaphores, conditions, atomic primitives, BMP loaders).
*/

#ifndef _SDL_compat_h
#define _SDL_compat_h

#include "SDL_types.h"

#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ----- CD-ROM (no optical drive on TCMIPS) --------------------------- */

SDL_DECLSPEC int  SDLCALL SDL_CDNumDrives(void);
SDL_DECLSPEC const char * SDLCALL SDL_CDName(int drive);
SDL_DECLSPEC void * SDLCALL SDL_CDOpen(int drive);
SDL_DECLSPEC void SDLCALL SDL_CDClose(void *cdrom);
SDL_DECLSPEC int  SDLCALL SDL_CDStatus(void *cdrom);
SDL_DECLSPEC int  SDLCALL SDL_CDPlay(void *cdrom, int start, int length);
SDL_DECLSPEC int  SDLCALL SDL_CDPlayTracks(void *cdrom,
                                            int strack, int sframe,
                                            int ntracks, int nframes);
SDL_DECLSPEC int  SDLCALL SDL_CDPause(void *cdrom);
SDL_DECLSPEC int  SDLCALL SDL_CDResume(void *cdrom);
SDL_DECLSPEC int  SDLCALL SDL_CDStop(void *cdrom);
SDL_DECLSPEC int  SDLCALL SDL_CDEject(void *cdrom);

/* ----- Atomic ------------------------------------------------------- */

SDL_DECLSPEC int SDLCALL SDL_AtomicAdd(volatile int *a, int v);
SDL_DECLSPEC int SDLCALL SDL_AtomicSet(volatile int *a, int v);
SDL_DECLSPEC int SDLCALL SDL_AtomicGet(volatile int *a);
SDL_DECLSPEC int SDLCALL SDL_AtomicIncrement(volatile int *a);
SDL_DECLSPEC int SDLCALL SDL_AtomicDecrement(volatile int *a);

/* ----- Threads (stubs) ---------------------------------------------- */

SDL_DECLSPEC void * SDLCALL SDL_CreateThread(int (SDLCALL *fn)(void *), void *data);
SDL_DECLSPEC Uint32 SDLCALL SDL_GetThreadID(void *thread);
SDL_DECLSPEC void   SDLCALL SDL_WaitThread(void *thread, int *status);
SDL_DECLSPEC void   SDLCALL SDL_KillThread(void *thread);

/* ----- Mutex (stubs) ------------------------------------------------ */

SDL_DECLSPEC void * SDLCALL SDL_CreateMutex(void);
SDL_DECLSPEC int    SDLCALL SDL_mutexP(void *mutex);
SDL_DECLSPEC int    SDLCALL SDL_mutexV(void *mutex);
SDL_DECLSPEC void   SDLCALL SDL_DestroyMutex(void *mutex);

/* ----- Semaphore (stubs) -------------------------------------------- */

SDL_DECLSPEC void * SDLCALL SDL_CreateSemaphore(Uint32 initial_value);
SDL_DECLSPEC void   SDLCALL SDL_DestroySemaphore(void *sem);
SDL_DECLSPEC int    SDLCALL SDL_SemWait(void *sem);
SDL_DECLSPEC int    SDLCALL SDL_SemTryWait(void *sem);
SDL_DECLSPEC int    SDLCALL SDL_SemWaitTimeout(void *sem, Uint32 ms);
SDL_DECLSPEC int    SDLCALL SDL_SemPost(void *sem);
SDL_DECLSPEC Uint32 SDLCALL SDL_SemValue(void *sem);

/* ----- Condition (stubs) -------------------------------------------- */

SDL_DECLSPEC void * SDLCALL SDL_CreateCond(void);
SDL_DECLSPEC void   SDLCALL SDL_DestroyCond(void *cond);
SDL_DECLSPEC int    SDLCALL SDL_CondSignal(void *cond);
SDL_DECLSPEC int    SDLCALL SDL_CondBroadcast(void *cond);
SDL_DECLSPEC int    SDLCALL SDL_CondWait(void *cond, void *mutex);
SDL_DECLSPEC int    SDLCALL SDL_CondWaitTimeout(void *cond, void *mutex, Uint32 ms);

/* ----- Dynamic loading (stubs) -------------------------------------- */

SDL_DECLSPEC void * SDLCALL SDL_LoadObject(const char *sofile);
SDL_DECLSPEC void * SDLCALL SDL_LoadFunction(void *handle, const char *name);
SDL_DECLSPEC void   SDLCALL SDL_UnloadObject(void *handle);

/* ----- BMP loader / saver ------------------------------------------- */

SDL_DECLSPEC SDL_Surface * SDLCALL SDL_LoadBMP_RW(SDL_RWops *src, int freesrc);
SDL_DECLSPEC int SDLCALL SDL_SaveBMP_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst);
#ifndef SDL_LoadBMP
#define SDL_LoadBMP(file) SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1)
#endif
#ifndef SDL_SaveBMP
#define SDL_SaveBMP(surface, file) SDL_SaveBMP_RW((surface), SDL_RWFromFile((file), "wb"), 1)
#endif

/* ----- Gamma (software ramp; hardware gamma is unavailable) ---------- */

SDL_DECLSPEC int SDLCALL SDL_SetGamma(float red, float green, float blue);
SDL_DECLSPEC int SDLCALL SDL_SetGammaRamp(Uint16 *red, Uint16 *green, Uint16 *blue);
SDL_DECLSPEC int SDLCALL SDL_GetGammaRamp(Uint16 *red, Uint16 *green, Uint16 *blue);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_compat_h */