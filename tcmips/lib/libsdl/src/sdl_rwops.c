/* SDL compatibility layer: RWops.
 *
 * File-backed RWops are served directly from the TCMIPS in-memory filesystem
 * (tcmips/lib/libmemfs, memfs_open_base / read / write / lseek / close) so that
 * this layer does not depend on the SDL2 compatibility layer.  In-memory RWops
 * are implemented here as well.
 *
 * Upstream SDL-1.2 baseline: release-1.2.15,
 * commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
 */

#include "tcmips_sdl_internal.h"

/* File-backed RWops use the libc (picolibc) POSIX file verbs; the libc
 * syscall glue in the BSP startup layer routes to the in-memory filesystem.
 * libsdl must not depend on libmemfs internals (fd base offsets, etc.). */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TCMIPS_SDL_RWOPS_FILE 1
#define TCMIPS_SDL_RWOPS_MEM 2

/* Packed into the 4 Void* SDL_RWops.hidden slots (16 bytes on a 32-bit
 * target): one pointer, one offset, one size and one flags word. */
typedef struct {
  void *base; /* MEM: memory base; FILE: cast of the libc fd */
  long pos;   /* current byte offset */
  int size;   /* MEM: buffer byte size; FILE: file size */
  int flags;  /* bit0 writable, bit1 is_file                  */
} tcmips_sdl_rw_hidden_t;

static int tcmips_sdl_rw_seek_fn(struct SDL_RWops *context, int offset,
                                 int whence) {
  tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)context->hidden;

  if (context->type == TCMIPS_SDL_RWOPS_FILE) {
    long rc = (long)lseek((int)(intptr_t)h->base, (off_t)offset, whence);
    if (rc < 0)
      return -1;
    h->pos = rc;
    return (int)rc;
  }

  /* Memory rwops. */
  int new_pos = (int)h->pos;
  if (whence == 0)
    new_pos = offset;
  else if (whence == 1)
    new_pos = (int)h->pos + offset;
  else if (whence == 2)
    new_pos = h->size + offset;
  if (new_pos < 0)
    new_pos = 0;
  h->pos = new_pos;
  return new_pos;
}

static int tcmips_sdl_rw_read_fn(struct SDL_RWops *context, void *ptr, int size,
                                 int maxnum) {
  tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)context->hidden;

  if (context->type == TCMIPS_SDL_RWOPS_FILE) {
    int fd = (int)(intptr_t)h->base;
    int want = size * maxnum;
    if (want <= 0)
      return 0;
    int got = (int)read(fd, ptr, (size_t)want);
    if (got <= 0)
      return 0;
    h->pos += got;
    return got / size;
  }

  /* Memory rwops. */
  int max = h->size - (int)h->pos;
  if (max > size * maxnum)
    max = size * maxnum;
  if (max < 0)
    max = 0;
  SDL_memcpy(ptr, (const char *)h->base + h->pos, (size_t)max);
  h->pos += max;
  return size > 0 ? max / size : 0;
}

static int tcmips_sdl_rw_write_fn(struct SDL_RWops *context, const void *ptr,
                                  int size, int num) {
  tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)context->hidden;

  if (context->type == TCMIPS_SDL_RWOPS_FILE) {
    int fd = (int)(intptr_t)h->base;
    int want = size * num;
    long got = (long)write(fd, ptr, (size_t)want);
    if (got <= 0)
      return 0;
    h->pos += got;
    return (int)got / size;
  }

  /* Memory rwops. */
  if (!(h->flags & 0x1)) {
    SDL_SetError("Cannot write to read-only memory RWops");
    return 0;
  }
  if ((int)h->pos + size * num > h->size) {
    SDL_SetError("Out of bounds memory RWops write");
    return 0;
  }
  SDL_memcpy((char *)h->base + h->pos, ptr, (size_t)size * num);
  h->pos += size * num;
  return num;
}

static int tcmips_sdl_rw_close_fn(struct SDL_RWops *context) {
  int rc = 0;
  if (context->type == TCMIPS_SDL_RWOPS_FILE) {
    tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)context->hidden;
    rc = close((int)(intptr_t)h->base);
  }
  SDL_free(context);
  return rc;
}

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode) {
  if (!file)
    return NULL;

  int flags = O_RDONLY;
  if (mode) {
    flags = 0;
    if (SDL_strchr(mode, 'r'))
      flags |= O_RDONLY;
    if (SDL_strchr(mode, 'w'))
      flags |= O_WRONLY | O_CREAT | O_TRUNC;
    if (SDL_strchr(mode, 'a'))
      flags |= O_WRONLY | O_CREAT | O_APPEND;
    if (SDL_strchr(mode, '+'))
      flags = (flags & ~O_RDONLY) | O_RDWR;
  }

  /* libc open(): fds 0/1/2 belong to the BSP console (fixed at startup). */
  int fd = open(file, flags, 0666);
  if (fd < 0) {
    SDL_SetError("SDL1: cannot open file '%s'", file);
    return NULL;
  }

  /* File size via lseek(SEEK_END) — no path-based stat needed. */
  off_t size = lseek(fd, 0, SEEK_END);
  if (size < 0)
    size = 0;
  lseek(fd, 0, SEEK_SET);

  SDL_RWops *rw = (SDL_RWops *)SDL_malloc(sizeof(SDL_RWops));
  if (!rw) {
    close(fd);
    return NULL;
  }
  SDL_memset(rw, 0, sizeof(*rw));
  tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)rw->hidden;
  rw->type = TCMIPS_SDL_RWOPS_FILE;
  rw->seek = tcmips_sdl_rw_seek_fn;
  rw->read = tcmips_sdl_rw_read_fn;
  rw->write = tcmips_sdl_rw_write_fn;
  rw->close = tcmips_sdl_rw_close_fn;
  h->base = (void *)(intptr_t)fd;
  h->pos = 0;
  h->size = (int)size;
  h->flags = 0x2; /* is_file */
  return rw;
}

SDL_RWops *SDL_RWFromFP(void *fp, int autoclose) {
  (void)fp;
  (void)autoclose;
  SDL_SetError("SDL_RWFromFP not available on TCMIPS");
  return NULL;
}

static SDL_RWops *tcmips_sdl_rw_from_mem_int(const void *mem, int size,
                                             int writable) {
  if (!mem || size <= 0)
    return NULL;
  SDL_RWops *rw = (SDL_RWops *)SDL_malloc(sizeof(SDL_RWops));
  if (!rw)
    return NULL;
  SDL_memset(rw, 0, sizeof(*rw));
  tcmips_sdl_rw_hidden_t *h = (tcmips_sdl_rw_hidden_t *)rw->hidden;
  rw->type = TCMIPS_SDL_RWOPS_MEM;
  rw->seek = tcmips_sdl_rw_seek_fn;
  rw->read = tcmips_sdl_rw_read_fn;
  rw->write = tcmips_sdl_rw_write_fn;
  rw->close = tcmips_sdl_rw_close_fn;
  h->base = (void *)mem;
  h->pos = 0;
  h->size = size;
  h->flags = writable ? 0x1 : 0x0;
  return rw;
}

SDL_RWops *SDL_RWFromMem(void *mem, int size) {
  return tcmips_sdl_rw_from_mem_int(mem, size, 1);
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) {
  return tcmips_sdl_rw_from_mem_int(mem, size, 0);
}

SDL_RWops *SDL_AllocRW(void) {
  SDL_RWops *rw = (SDL_RWops *)SDL_malloc(sizeof(SDL_RWops));
  if (!rw)
    return NULL;
  SDL_memset(rw, 0, sizeof(*rw));
  rw->type = 0;
  rw->seek = tcmips_sdl_rw_seek_fn;
  rw->read = tcmips_sdl_rw_read_fn;
  rw->write = tcmips_sdl_rw_write_fn;
  rw->close = tcmips_sdl_rw_close_fn;
  return rw;
}

void SDL_FreeRW(SDL_RWops *area) {
  if (area)
    SDL_free(area);
}