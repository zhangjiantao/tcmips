#include <SDL.h>

#include <stdarg.h>
#include <stdio.h>

#define TCMIPS_SDL_NUM_LOG_CATEGORIES (SDL_LOG_CATEGORY_CUSTOM + 1)

static int s_log_priorities[TCMIPS_SDL_NUM_LOG_CATEGORIES] = {0};

void SDL_LogSetPriority(int category, SDL_LogPriority priority) {
  if (category < 0 || category >= TCMIPS_SDL_NUM_LOG_CATEGORIES)
    return;
  s_log_priorities[category] = priority;
}

SDL_LogPriority SDL_LogGetPriority(int category) {
  if (category < 0 || category >= TCMIPS_SDL_NUM_LOG_CATEGORIES)
    return SDL_LOG_PRIORITY_INFO;
  return (SDL_LogPriority)s_log_priorities[category];
}

void SDL_LogResetPriorities(void) {
  for (int i = 0; i < TCMIPS_SDL_NUM_LOG_CATEGORIES; ++i) {
    s_log_priorities[i] = 0;
  }
}

void SDL_LogMessageV(int category, SDL_LogPriority priority, const char *fmt,
                     va_list ap) {
  (void)category;
  if (!fmt)
    return;
  char buf[512];
  SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
  printf("[SDL][%d] %s\n", priority, buf);
}

void SDL_LogMessage(int category, SDL_LogPriority priority, const char *fmt,
                    ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, priority, fmt, ap);
  va_end(ap);
}

void SDL_Log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
  va_end(ap);
}

void SDL_LogVerbose(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_VERBOSE, fmt, ap);
  va_end(ap);
}

void SDL_LogDebug(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_DEBUG, fmt, ap);
  va_end(ap);
}

void SDL_LogInfo(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, ap);
  va_end(ap);
}

void SDL_LogWarn(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_WARN, fmt, ap);
  va_end(ap);
}

void SDL_LogError(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, ap);
  va_end(ap);
}

void SDL_LogCritical(int category, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_CRITICAL, fmt, ap);
  va_end(ap);
}

void SDL_LogSetOutputFunction(SDL_LogOutputFunction callback, void *userdata) {
  // TCMIPS only supports stdout output; storage callbacks not implemented.
  (void)callback;
  (void)userdata;
}

void SDL_LogGetOutputFunction(SDL_LogOutputFunction *callback,
                              void **userdata) {
  if (callback)
    *callback = NULL;
  if (userdata)
    *userdata = NULL;
}
