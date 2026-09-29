/* TCMIPS BSP addition: minimal <sys/statvfs.h>.

Stock picolibc ships no <sys/statvfs.h>. libc++'s <filesystem> includes it
unconditionally (on non-Windows) and reads struct statvfs::{f_frsize,f_blocks,
f_bavail} in space(). TCMIPS's storage is a RAM-backed memfs, so a single
"volume" is reported: 1-byte blocks, f_blocks = total in-memory FS size and
f_bavail = still-free. The actual numbers are filled in by the BSP's statvfs().
*/

#ifndef _SYS_STATVFS_H_
#define _SYS_STATVFS_H_

#include <sys/types.h> /* fsblkcnt_t / fsfilcnt_t */

#ifdef __cplusplus
extern "C" {
#endif

struct statvfs {
  unsigned long f_bsize;   /* fundamental file system block size */
  unsigned long f_frsize;  /* fragment size */
  fsblkcnt_t   f_blocks;   /* total blocks in FS */
  fsblkcnt_t   f_bfree;    /* free blocks */
  fsblkcnt_t   f_bavail;   /* free blocks for unprivileged users */
  fsfilcnt_t   f_files;    /* total file nodes */
  fsfilcnt_t   f_ffree;    /* free file nodes */
  unsigned long f_fsid;    /* file system id */
  unsigned long f_flag;    /* mount flags */
  unsigned long f_namemax; /* maximum filename length */
};

int statvfs(const char *__restrict path, struct statvfs *__restrict buf);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STATVFS_H_ */
