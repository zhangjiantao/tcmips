#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>

#include <dev/syscall.h>

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback,
                         void *param) {
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) { return 1; }

static uint32_t ts = 0;
uint32_t SDL_GetTicks() {
  if (!ts)
    ts = tcm_syscall_get_timestamp();

  return (tcm_syscall_get_timestamp() - ts) * 1000 +
         tcm_syscall_get_timestamp_milli();
}

void SDL_Delay(uint32_t ms) {
  // ms /= 4;
  // if (ms == 0) return;
  // uint32_t start_ms = tcm_syscall_get_timestamp_milli();
  // while ((tcm_syscall_get_timestamp_milli() - start_ms) < ms) {
  // }
}
