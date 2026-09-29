#include <SDL.h>

const char *SDL_GetPlatform(void) { return "TCMIPS"; }

int SDL_GetCPUCount(void) { return 1; }

SDL_bool SDL_Has3DNow(void) { return SDL_FALSE; }
SDL_bool SDL_HasAVX(void) { return SDL_FALSE; }
SDL_bool SDL_HasAVX2(void) { return SDL_FALSE; }
SDL_bool SDL_HasAVX512F(void) { return SDL_FALSE; }
SDL_bool SDL_HasAltiVec(void) { return SDL_FALSE; }
SDL_bool SDL_HasMMX(void) { return SDL_FALSE; }
SDL_bool SDL_HasRDTSC(void) { return SDL_FALSE; }
SDL_bool SDL_HasSSE(void) { return SDL_FALSE; }
SDL_bool SDL_HasSSE2(void) { return SDL_FALSE; }
SDL_bool SDL_HasSSE3(void) { return SDL_FALSE; }
SDL_bool SDL_HasSSE41(void) { return SDL_FALSE; }
SDL_bool SDL_HasSSE42(void) { return SDL_FALSE; }
SDL_bool SDL_HasNEON(void) { return SDL_FALSE; }
