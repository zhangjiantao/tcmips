#include "tcmips_sdl2_internal.h"
#include <SDL.h>

/* File-backed RWops go through the libc (picolibc) POSIX file verbs
 * (open/read/write/lseek/close).  The libc syscall glue lives in the BSP
 * (tcmips/bsp/posix.c) which routes to the in-memory filesystem — libsdl2
 * itself must not (and does not) depend on libmemfs internals such as the
 * fd base offset. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TCMIPS_SDL_RWOPS_MEM_TYPE 1
#define TCMIPS_SDL_RWOPS_DIRECT_TYPE 2 // direct memory (SDL_RWFromMem/ConstMem)

typedef struct {
  int fd;
  Sint64 size;
  Sint64 pos;
} tcmips_sdl2_rwops_mem;

typedef struct {
  const void *ptr; // const: FromConstMem keeps it const; FromMem casts away
  Sint64 size;
  Sint64 pos;
  SDL_bool writable;
} tcmips_sdl2_rwops_direct;

/* The per-context state lives in its own heap allocation, surfaced through
 * `hidden.unknown.data1`.  The upstream `hidden` union only offers 12 bytes
 * (mem{base,here,stop} vs unknown{data1,data2} = 8), while these context
 * structs are 24/28 bytes: overlaying them directly onto `ops->hidden`
 * overflowed the 36-byte SDL_RWops allocation and corrupted the adjacent
 * heap chunk (picolibc __malloc_free_list node), manifesting as a wild
 * `lw` from a pointer of 1/-3 later on. */

static tcmips_sdl2_rwops_mem *tcmips_sdl2_rwops_mem_of(SDL_RWops *context) {
  return (tcmips_sdl2_rwops_mem *)context->hidden.unknown.data1;
}

static tcmips_sdl2_rwops_direct *
tcmips_sdl2_rwops_direct_of(SDL_RWops *context) {
  return (tcmips_sdl2_rwops_direct *)context->hidden.unknown.data1;
}

static Sint64 SDLCALL tcmips_sdl2_rwops_size(SDL_RWops *context) {
  if (!context || context->type != TCMIPS_SDL_RWOPS_MEM_TYPE)
    return -1;
  tcmips_sdl2_rwops_mem *m = tcmips_sdl2_rwops_mem_of(context);
  return m->size;
}

static Sint64 SDLCALL tcmips_sdl2_rwops_seek(SDL_RWops *context, Sint64 offset,
                                             int whence) {
  if (!context || context->type != TCMIPS_SDL_RWOPS_MEM_TYPE)
    return -1;
  tcmips_sdl2_rwops_mem *m = tcmips_sdl2_rwops_mem_of(context);
  Sint64 newpos = m->pos;
  switch (whence) {
  case RW_SEEK_SET:
    newpos = offset;
    break;
  case RW_SEEK_CUR:
    newpos = m->pos + offset;
    break;
  case RW_SEEK_END:
    newpos = m->size + offset;
    break;
  }
  if (newpos < 0)
    newpos = 0;
  if (newpos > m->size)
    newpos = m->size;
  /* Keep the libc fd position in sync: read()/write() below use the fd's
     own offset. */
  lseek(m->fd, (off_t)newpos, SEEK_SET);
  m->pos = newpos;
  return m->pos;
}

static size_t SDLCALL tcmips_sdl2_rwops_read(SDL_RWops *context, void *ptr,
                                             size_t size, size_t maxnum) {
  if (!context || !ptr || size == 0 || maxnum == 0)
    return 0;
  tcmips_sdl2_rwops_mem *m = tcmips_sdl2_rwops_mem_of(context);
  if (m->pos >= m->size)
    return 0;

  Sint64 bytes_left = m->size - m->pos;
  Sint64 want = (Sint64)size * (Sint64)maxnum;
  if (want > bytes_left)
    want = bytes_left;
  if (want <= 0)
    return 0;

  // The file offset in the context always mirrors libc's fd position.
  ssize_t got = read(m->fd, ptr, (size_t)want);
  if (got < 0)
    return 0;
  m->pos += got;
  return (size_t)(got / size); // number of items
}

static size_t SDLCALL tcmips_sdl2_rwops_write(SDL_RWops *context,
                                              const void *ptr, size_t size,
                                              size_t num) {
  if (!context || !ptr || size == 0 || num == 0)
    return 0;
  tcmips_sdl2_rwops_mem *m = tcmips_sdl2_rwops_mem_of(context);
  size_t total = size * num;
  ssize_t got = write(m->fd, ptr, total);
  if (got < 0)
    return 0;
  m->pos += got;
  if (m->pos > m->size)
    m->size = m->pos;
  return (size_t)(got / size);
}

static int SDLCALL tcmips_sdl2_rwops_close(SDL_RWops *context) {
  if (!context)
    return -1;
  if (context->type == TCMIPS_SDL_RWOPS_MEM_TYPE) {
    tcmips_sdl2_rwops_mem *m = tcmips_sdl2_rwops_mem_of(context);
    if (m) {
      close(m->fd);
      SDL_free(m);
    }
  }
  SDL_free(context);
  return 0;
}

// ---- direct-memory RWops callbacks (SDL_RWFromMem / SDL_RWFromConstMem) ----

static Sint64 SDLCALL tcmips_sdl2_rwops_direct_size(SDL_RWops *context) {
  if (!context || context->type != TCMIPS_SDL_RWOPS_DIRECT_TYPE)
    return -1;
  tcmips_sdl2_rwops_direct *d = tcmips_sdl2_rwops_direct_of(context);
  return d->size;
}

static Sint64 SDLCALL tcmips_sdl2_rwops_direct_seek(SDL_RWops *context,
                                                    Sint64 offset, int whence) {
  if (!context || context->type != TCMIPS_SDL_RWOPS_DIRECT_TYPE)
    return -1;
  tcmips_sdl2_rwops_direct *d = tcmips_sdl2_rwops_direct_of(context);
  Sint64 newpos = d->pos;
  switch (whence) {
  case RW_SEEK_SET:
    newpos = offset;
    break;
  case RW_SEEK_CUR:
    newpos = d->pos + offset;
    break;
  case RW_SEEK_END:
    newpos = d->size + offset;
    break;
  }
  if (newpos < 0)
    newpos = 0;
  if (newpos > d->size)
    newpos = d->size;
  d->pos = newpos;
  return d->pos;
}

static size_t SDLCALL tcmips_sdl2_rwops_direct_read(SDL_RWops *context,
                                                    void *ptr, size_t size,
                                                    size_t maxnum) {
  if (!context || !ptr || size == 0 || maxnum == 0)
    return 0;
  tcmips_sdl2_rwops_direct *d = tcmips_sdl2_rwops_direct_of(context);
  if (d->pos >= d->size)
    return 0;

  Sint64 bytes_left = d->size - d->pos;
  Sint64 want = (Sint64)size * (Sint64)maxnum;
  if (want > bytes_left)
    want = bytes_left;
  if (want <= 0)
    return 0;

  SDL_memcpy(ptr, (const Uint8 *)d->ptr + d->pos, (size_t)want);
  d->pos += want;
  return (size_t)(want / size);
}

static size_t SDLCALL tcmips_sdl2_rwops_direct_write(SDL_RWops *context,
                                                     const void *ptr,
                                                     size_t size, size_t num) {
  if (!context || !ptr || size == 0 || num == 0)
    return 0;
  tcmips_sdl2_rwops_direct *d = tcmips_sdl2_rwops_direct_of(context);
  if (!d->writable) {
    SDL_SetError("Attempt to write to a read-only (const) memory RWops");
    return 0;
  }
  Sint64 bytes_free = d->size - d->pos;
  Sint64 want = (Sint64)size * (Sint64)num;
  if (want > bytes_free)
    want = bytes_free;
  if (want <= 0)
    return 0;

  SDL_memcpy((Uint8 *)d->ptr + d->pos, ptr, (size_t)want);
  d->pos += want;
  return (size_t)(want / size);
}

static int SDLCALL tcmips_sdl2_rwops_direct_close(SDL_RWops *context) {
  if (!context)
    return -1;
  // Direct-memory RWops do not own the buffer; free the context + the ops.
  SDL_free(tcmips_sdl2_rwops_direct_of(context));
  SDL_free(context);
  return 0;
}

static SDL_RWops *tcmips_sdl2_alloc_rwops(const char *file, const char *mode) {
  int flags = 0;
  if (mode) {
    if (SDL_strchr(mode, 'r'))
      flags |= O_RDONLY;
    if (SDL_strchr(mode, 'w'))
      flags |= O_WRONLY | O_CREAT | O_TRUNC;
    if (SDL_strchr(mode, 'a'))
      flags |= O_WRONLY | O_CREAT | O_APPEND;
    if (SDL_strchr(mode, '+'))
      flags = (flags & ~O_RDONLY) | O_RDWR;
  } else {
    flags = O_RDONLY;
  }

  // The BSP reserves fds 0/1/2 (stdin/stdout/stderr) for the console and
  // hands libmemfs a base of 3 at init (see tcm_bsp.h MEMFS_BASE_FD); open()
  // here is the BSP's own POSIX open, which simply returns libmemfs fds >= 3.
  int fd = open(file, flags, 0666);
  if (fd < 0) {
    SDL_SetError("SDL2: cannot open file '%s'", file);
    return NULL;
  }

  /* File size via lseek(SEEK_END) — works on any seekable libc fd without
     needing path-based stat support. */
  off_t size = lseek(fd, 0, SEEK_END);
  if (size < 0)
    size = 0;
  lseek(fd, 0, SEEK_SET);

  SDL_RWops *ops = (SDL_RWops *)SDL_calloc(1, sizeof(SDL_RWops));
  if (!ops) {
    close(fd);
    return NULL;
  }

  ops->type = TCMIPS_SDL_RWOPS_MEM_TYPE;
  tcmips_sdl2_rwops_mem *m = (tcmips_sdl2_rwops_mem *)SDL_calloc(1, sizeof(*m));
  if (!m) {
    close(fd);
    SDL_free(ops);
    return NULL;
  }
  m->fd = fd;
  m->size = (Sint64)size;
  m->pos = 0;
  ops->hidden.unknown.data1 = m;

  ops->size = tcmips_sdl2_rwops_size;
  ops->seek = tcmips_sdl2_rwops_seek;
  ops->read = tcmips_sdl2_rwops_read;
  ops->write = tcmips_sdl2_rwops_write;
  ops->close = tcmips_sdl2_rwops_close;
  return ops;
}

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode) {
  return tcmips_sdl2_alloc_rwops(file, mode);
}

SDL_RWops *SDL_RWFromMem(void *mem, int size) {
  if (!mem || size <= 0)
    return NULL;

  SDL_RWops *ops = (SDL_RWops *)SDL_calloc(1, sizeof(SDL_RWops));
  if (!ops) {
    SDL_OutOfMemory();
    return NULL;
  }

  ops->type = TCMIPS_SDL_RWOPS_DIRECT_TYPE;
  tcmips_sdl2_rwops_direct *d =
      (tcmips_sdl2_rwops_direct *)SDL_calloc(1, sizeof(*d));
  if (!d) {
    SDL_free(ops);
    SDL_OutOfMemory();
    return NULL;
  }
  d->ptr = mem;
  d->size = size;
  d->pos = 0;
  d->writable = SDL_TRUE;
  ops->hidden.unknown.data1 = d;

  ops->size = tcmips_sdl2_rwops_direct_size;
  ops->seek = tcmips_sdl2_rwops_direct_seek;
  ops->read = tcmips_sdl2_rwops_direct_read;
  ops->write = tcmips_sdl2_rwops_direct_write;
  ops->close = tcmips_sdl2_rwops_direct_close;
  return ops;
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) {
  // Same as FromMem, but the returned RWops will reject writes.
  SDL_RWops *ops = SDL_RWFromMem((void *)mem, size);
  if (ops) {
    tcmips_sdl2_rwops_direct *d = tcmips_sdl2_rwops_direct_of(ops);
    d->writable = SDL_FALSE;
  }
  return ops;
}

SDL_RWops *SDL_AllocRW(void) {
  return tcmips_sdl2_alloc_rwops("/__sdl_memfs__/anonymous", "w+");
}

void SDL_FreeRW(SDL_RWops *area) {
  if (area) {
    area->close(area);
  }
}

Uint8 SDL_ReadU8(SDL_RWops *src) {
  Uint8 v = 0;
  SDL_RWread(src, &v, 1, 1);
  return v;
}
Uint16 SDL_ReadLE16(SDL_RWops *src) {
  Uint16 v = 0;
  Uint8 b[2];
  if (SDL_RWread(src, b, 2, 1) == 1)
    v = ((Uint16)b[0]) | (((Uint16)b[1]) << 8);
  return v;
}
Uint16 SDL_ReadBE16(SDL_RWops *src) {
  Uint16 v = 0;
  Uint8 b[2];
  if (SDL_RWread(src, b, 2, 1) == 1)
    v = ((Uint16)b[1]) | (((Uint16)b[0]) << 8);
  return v;
}
Uint32 SDL_ReadLE32(SDL_RWops *src) {
  Uint32 v = 0;
  Uint8 b[4];
  if (SDL_RWread(src, b, 4, 1) == 1) {
    v = ((Uint32)b[0]) | (((Uint32)b[1]) << 8) | (((Uint32)b[2]) << 16) |
        (((Uint32)b[3]) << 24);
  }
  return v;
}
Uint32 SDL_ReadBE32(SDL_RWops *src) {
  Uint32 v = 0;
  Uint8 b[4];
  if (SDL_RWread(src, b, 4, 1) == 1) {
    v = ((Uint32)b[3]) | (((Uint32)b[2]) << 8) | (((Uint32)b[1]) << 16) |
        (((Uint32)b[0]) << 24);
  }
  return v;
}
Uint64 SDL_ReadLE64(SDL_RWops *src) {
  Uint64 v = 0;
  Uint8 b[8];
  if (SDL_RWread(src, b, 8, 1) == 1) {
    for (int i = 0; i < 8; ++i)
      v |= ((Uint64)b[i]) << (i * 8);
  }
  return v;
}
Uint64 SDL_ReadBE64(SDL_RWops *src) {
  Uint64 v = 0;
  Uint8 b[8];
  if (SDL_RWread(src, b, 8, 1) == 1) {
    for (int i = 0; i < 8; ++i)
      v |= ((Uint64)b[7 - i]) << (i * 8);
  }
  return v;
}

size_t SDL_WriteU8(SDL_RWops *dst, Uint8 value) {
  return SDL_RWwrite(dst, &value, 1, 1);
}
size_t SDL_WriteLE16(SDL_RWops *dst, Uint16 value) {
  Uint8 b[2] = {(Uint8)(value & 0xFF), (Uint8)((value >> 8) & 0xFF)};
  return SDL_RWwrite(dst, b, 2, 1);
}
size_t SDL_WriteBE16(SDL_RWops *dst, Uint16 value) {
  Uint8 b[2] = {(Uint8)((value >> 8) & 0xFF), (Uint8)(value & 0xFF)};
  return SDL_RWwrite(dst, b, 2, 1);
}
size_t SDL_WriteLE32(SDL_RWops *dst, Uint32 value) {
  Uint8 b[4] = {(Uint8)(value & 0xFF), (Uint8)((value >> 8) & 0xFF),
                (Uint8)((value >> 16) & 0xFF), (Uint8)((value >> 24) & 0xFF)};
  return SDL_RWwrite(dst, b, 4, 1);
}
size_t SDL_WriteBE32(SDL_RWops *dst, Uint32 value) {
  Uint8 b[4] = {(Uint8)((value >> 24) & 0xFF), (Uint8)((value >> 16) & 0xFF),
                (Uint8)((value >> 8) & 0xFF), (Uint8)(value & 0xFF)};
  return SDL_RWwrite(dst, b, 4, 1);
}
size_t SDL_WriteLE64(SDL_RWops *dst, Uint64 value) {
  Uint8 b[8];
  for (int i = 0; i < 8; ++i)
    b[i] = (Uint8)((value >> (i * 8)) & 0xFF);
  return SDL_RWwrite(dst, b, 8, 1);
}
size_t SDL_WriteBE64(SDL_RWops *dst, Uint64 value) {
  Uint8 b[8];
  for (int i = 0; i < 8; ++i)
    b[i] = (Uint8)((value >> ((7 - i) * 8)) & 0xFF);
  return SDL_RWwrite(dst, b, 8, 1);
}

Sint64 SDL_RWsize(SDL_RWops *context) {
  if (!context || !context->size)
    return -1;
  return context->size(context);
}

Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence) {
  if (!context || !context->seek)
    return -1;
  return context->seek(context, offset, whence);
}

Sint64 SDL_RWtell(SDL_RWops *context) {
  return SDL_RWseek(context, 0, RW_SEEK_CUR);
}

size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum) {
  if (!context || !context->read)
    return 0;
  return context->read(context, ptr, size, maxnum);
}

size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size,
                   size_t num) {
  if (!context || !context->write)
    return 0;
  return context->write(context, ptr, size, num);
}

int SDL_RWclose(SDL_RWops *context) {
  if (!context || !context->close)
    return -1;
  return context->close(context);
}

void *SDL_LoadFile_RW(SDL_RWops *src, size_t *datasize, int freesrc) {
  if (!src)
    return NULL;
  Sint64 sz = SDL_RWsize(src);
  if (sz < 0) {
    if (freesrc)
      SDL_RWclose(src);
    return NULL;
  }
  void *data = SDL_malloc((size_t)sz + 1);
  if (!data) {
    if (freesrc)
      SDL_RWclose(src);
    return NULL;
  }
  SDL_RWseek(src, 0, RW_SEEK_SET);
  size_t read = SDL_RWread(src, data, 1, (size_t)sz);
  if (datasize)
    *datasize = read;
  ((char *)data)[read] = '\0';
  if (freesrc)
    SDL_RWclose(src);
  return data;
}

void *SDL_LoadFile(const char *file, size_t *datasize) {
  SDL_RWops *src = SDL_RWFromFile(file, "rb");
  if (!src)
    return NULL;
  return SDL_LoadFile_RW(src, datasize, 1);
}
