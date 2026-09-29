/*
  Simple DirectMedia Layer 1.2 compatibility headers for TCMIPS
  This file mirrors the upstream SDL-1.2 SDL_rwops.h layout.
  Upstream reference: release-1.2.15, commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
*/

#ifndef _SDL_rwops_h
#define _SDL_rwops_h

#include "SDL_types.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_RWops {
    /* Seek to 'offset' of 'whence', returns final position. */
    int (SDLCALL *seek)(struct SDL_RWops *context, int offset, int whence);

    /* Read up to 'num' objects of size 'objsize' into 'ptr'. */
    int (SDLCALL *read)(struct SDL_RWops *context, void *ptr, int size, int maxnum);

    /* Write up to 'num' objects of size 'objsize' from 'ptr'. */
    int (SDLCALL *write)(struct SDL_RWops *context, const void *ptr, int size, int num);

    /* Close and free the RWops */
    int (SDLCALL *close)(struct SDL_RWops *context);

    Uint32 type;
    void *hidden[4];
} SDL_RWops;

SDL_DECLSPEC SDL_RWops * SDLCALL SDL_RWFromFile(const char *file, const char *mode);
SDL_DECLSPEC SDL_RWops * SDLCALL SDL_RWFromFP(void *fp, int autoclose);
SDL_DECLSPEC SDL_RWops * SDLCALL SDL_RWFromMem(void *mem, int size);
SDL_DECLSPEC SDL_RWops * SDLCALL SDL_RWFromConstMem(const void *mem, int size);
SDL_DECLSPEC SDL_RWops * SDLCALL SDL_AllocRW(void);
SDL_DECLSPEC void SDLCALL SDL_FreeRW(SDL_RWops *area);

#define SDL_RWseek(ctx, offset, whence)	(ctx)->seek(ctx, offset, whence)
#define SDL_RWread(ctx, ptr, size, n)		(ctx)->read(ctx, ptr, size, n)
#define SDL_RWwrite(ctx, ptr, size, n)		(ctx)->write(ctx, ptr, size, n)
#define SDL_RWclose(ctx)					(ctx)->close(ctx)

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_rwops_h */
