/*
Copyright (c) 1982, 1986, 1993
The Regents of the University of California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. Neither the name of the University nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
 */
/* TCMIPS BSP override: stock picolibc ships this file as a bare `#error`
   placeholder because freestanding / bare-metal targets normally have no
   directory support. TCMIPS exposes an in-memory filesystem (libmemfs) with
   POSIX-flavoured directory enumeration, so this override provides the real
   `struct dirent`, `DT_*` type tags and the `DIR` stream type that the
   top-level `<dirent.h>` declares functions over.

   It is deliberately routed into the BSP by editing this vendored header
   rather than shadowing the sysroot copy: the CMake "source -> sysroot" header
   sync then picks it up automatically on every configure, so the types are
   stable.

   `struct dirent` and `DT_*` follow the BSD layout; the layout is what libc++'s
   `<filesystem>` `__directory_iterator` expects (it reads `::readdir()`'s
   `d_name` and, when `DT_BLK` is defined, switches on `d_type`).
*/

#ifndef _SYS_DIRENT_H_
#define _SYS_DIRENT_H_

#include <sys/types.h> /* ino_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum length of a single directory entry name. The memfs catalog stores
   arbitrary-length names, so this only bounds the in-memory dirent buffer a
   directory stream returns readdir() results through; nothing truncates on
   storage. */
#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif

/* File type tags for struct dirent::d_type. Values are the traditional BSD
   d_type constants. libc++ keys its readdir file-type switch on DT_BLK. */
#define DT_UNKNOWN 0 /* unknown file type */
#define DT_FIFO    1 /* named pipe */
#define DT_CHR     2 /* character device */
#define DT_DIR     4 /* directory */
#define DT_BLK     6 /* block device */
#define DT_REG     8 /* regular file */
#define DT_LNK     10 /* symbolic link */
#define DT_SOCK    12 /* socket */

#define _DIRENT_HAVE_D_TYPE

/* Directory entry as returned by readdir(). d_ino is unused by memfs (set to
   0); d_type is a DT_* tag; d_name is NUL-terminated. */
struct dirent {
  ino_t d_ino;            /* file serial number (0 on memfs) */
  unsigned char d_type;   /* file type tag, one of DT_* */
  char d_name[MAXNAMLEN]; /* null-terminated name */
};

/* Directory stream. dd_off backs libc++'s seekdir/telldir/rewinddir semantics;
   the memfs BSP keeps its was-read cursor and returns entries via dd_dirent.
   The fields are private to the BSP and are not part of any public contract. */
typedef struct __dirstream {
  int dd_fd;              /* memfs fd used to track this stream, or -1 */
  long dd_off;            /* byte offset placeholder for seekdir/telldir */
  struct dirent dd_dirent; /* storage backing the pointer readdir() returns */
} DIR;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DIRENT_H_ */
