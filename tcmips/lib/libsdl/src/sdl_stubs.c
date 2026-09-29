/* SDL compatibility layer: stubs for functions not supported on TCMIPS.
 * Note: functions that are present in the TCMIPS SDL2 layer with compatible
 * signatures (SDL_GetError, SDL_qsort, SDL_atof, SDL_Swap*, ...) are
 * intentionally NOT defined here; they are resolved from tcmips_sdl2.
 */

#include "tcmips_sdl_internal.h"

#include <string.h>

/* Threads / mutex */
void *SDL_CreateThread(int(SDLCALL *fn)(void *), void *data) {
  (void)fn;
  (void)data;
  TCMIPS_SDL_STUB_MSG("SDL_CreateThread");
  return NULL;
}
Uint32 SDL_GetThreadID(void *thread) {
  (void)thread;
  return 0;
}
void SDL_WaitThread(void *thread, int *status) {
  (void)thread;
  if (status)
    *status = -1;
}
void SDL_KillThread(void *thread) { (void)thread; }

void *SDL_CreateMutex(void) {
  TCMIPS_SDL_STUB_MSG("SDL_CreateMutex");
  return NULL;
}
int SDL_mutexP(void *mutex) {
  (void)mutex;
  return -1;
}
int SDL_mutexV(void *mutex) {
  (void)mutex;
  return -1;
}
void SDL_DestroyMutex(void *mutex) { (void)mutex; }

void *SDL_CreateSemaphore(Uint32 initial_value) {
  (void)initial_value;
  TCMIPS_SDL_STUB_MSG("SDL_CreateSemaphore");
  return NULL;
}
void SDL_DestroySemaphore(void *sem) { (void)sem; }
int SDL_SemWait(void *sem) {
  (void)sem;
  return -1;
}
int SDL_SemTryWait(void *sem) {
  (void)sem;
  return -1;
}
int SDL_SemWaitTimeout(void *sem, Uint32 ms) {
  (void)sem;
  (void)ms;
  return -1;
}
int SDL_SemPost(void *sem) {
  (void)sem;
  return -1;
}
Uint32 SDL_SemValue(void *sem) {
  (void)sem;
  return 0;
}

void *SDL_CreateCond(void) {
  TCMIPS_SDL_STUB_MSG("SDL_CreateCond");
  return NULL;
}
void SDL_DestroyCond(void *cond) { (void)cond; }
int SDL_CondSignal(void *cond) {
  (void)cond;
  return -1;
}
int SDL_CondBroadcast(void *cond) {
  (void)cond;
  return -1;
}
int SDL_CondWait(void *cond, void *mutex) {
  (void)cond;
  (void)mutex;
  return -1;
}
int SDL_CondWaitTimeout(void *cond, void *mutex, Uint32 ms) {
  (void)cond;
  (void)mutex;
  (void)ms;
  return -1;
}

/* Atomic stubs (SDL1 signature, not SDL2) */
int SDL_AtomicGet(volatile int *a) { return *a; }
int SDL_AtomicSet(volatile int *a, int v) {
  int old = *a;
  *a = v;
  return old;
}
int SDL_AtomicAdd(volatile int *a, int v) { return (*a += v); }
int SDL_AtomicDecrement(volatile int *a) { return --(*a); }
int SDL_AtomicIncrement(volatile int *a) { return ++(*a); }

/* Dynamic loading */
void *SDL_LoadObject(const char *sofile) {
  (void)sofile;
  TCMIPS_SDL_STUB_MSG("SDL_LoadObject");
  return NULL;
}
void *SDL_LoadFunction(void *handle, const char *name) {
  (void)handle;
  (void)name;
  return NULL;
}
void SDL_UnloadObject(void *handle) { (void)handle; }

/* Gamma ramp (TCMIPS keeps a stored ramp only; no hardware LUT) */
int SDL_SetGamma(float red, float green, float blue) {
  (void)red;
  (void)green;
  (void)blue;
  return 0;
}
int SDL_SetGammaRamp(Uint16 *red, Uint16 *green, Uint16 *blue) {
  int i;
  if (red)
    for (i = 0; i < 256; ++i)
      g_tcmips_sdl.gamma_ramp[0 * 256 + i] = red[i];
  if (green)
    for (i = 0; i < 256; ++i)
      g_tcmips_sdl.gamma_ramp[1 * 256 + i] = green[i];
  if (blue)
    for (i = 0; i < 256; ++i)
      g_tcmips_sdl.gamma_ramp[2 * 256 + i] = blue[i];
  return 0;
}
int SDL_GetGammaRamp(Uint16 *red, Uint16 *green, Uint16 *blue) {
  int i;
  if (red)
    for (i = 0; i < 256; ++i)
      red[i] = g_tcmips_sdl.gamma_ramp[0 * 256 + i];
  if (green)
    for (i = 0; i < 256; ++i)
      green[i] = g_tcmips_sdl.gamma_ramp[1 * 256 + i];
  if (blue)
    for (i = 0; i < 256; ++i)
      blue[i] = g_tcmips_sdl.gamma_ramp[2 * 256 + i];
  return 0;
}
