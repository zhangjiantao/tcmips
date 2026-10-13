//// libmemfs - In-memory filesystem runtime for TCMIPS (public BSP API).
//
// On the TCMIPS platform there is no disk, no MMU and no read-only memory:
// the entire .tcm image is plain RAM and every byte is writable. This module
// exposes a POSIX-flavoured BSP file layer (open/read/write/lseek/close/stat)
// backed by an in-memory file system whose file contents are burned into the
// image by the linker (llvm-link-tcmips --tcm-memfs=<dir>).
//
// It mirrors the spirit of Emscripten's MEMFS: files live in memory, are
// addressable by path at runtime, and are writable.
//
// Header split:
//   * memfs_abi.h - on-disk catalog layout (MemfsEntry / MemfsPayloadHeader),
//     the magic/version constants, and the runtime-visible path/name limits.
//     It is the ABI contract with the linker; this header re-exposes it via include.
//   * memfs.h     - the runtime POSIX BSP API below (this file).
//
// Semantics follow POSIX file descriptions:
//   * file CONTENT is globally shared across every open() of the same name
//     (an "file description" / inode view), including the packed segment0 RAM;
//   * each open() owns an independent file OFFSET and open mode.
//
// Memory model
//   .tcm image (unified RAM, all writable)
//   ├─ MemfsEntry[]         catalog records pointing into the payload
//   ├─ per-file segment0    the packed content, a fixed RAM region, writable
//   └─ (runtime) Chunk chain malloc'd blocks appended when writes exceed the
//      end of segment0; reference counted and freed on last close().
//
// The implementation never reallocs/relocates existing file data. Writes
// inside segment0 modify the packed RAM in place; writes past the end append
// fixed-size Chunk nodes to a shared chain.

#ifndef MEMFS_H
#define MEMFS_H

/* Bring in the project-level build configuration first, so that the
 * TCM_MEMFS_DEBUG flag (controlled by the project-level TCM_MEMFS_DEBUG switch in
 * the root CMakeLists.txt) is visible to this header and therefore to every
 * consumer, without needing a -D on the command line.  When TCM_MEMFS_DEBUG is
 * defined the debug helpers (memfs_debug_dump_tree / memfs_debug_heap_bytes
 * / memfs_debug_chunk_count) below are exposed; the libtcmips_memfs library
 * compiles the corresponding accounting code under `#ifdef TCM_MEMFS_DEBUG` and
 * exports the underlying accounting functions only in that build. */
#include <tcm_config.h>

#include "memfs_abi.h"

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// MEMFS_NAME_MAX / MEMFS_PATH_MAX are defined in memfs_abi.h (the shared
// contract with the linker) and are therefore also available here.

// Initialise the filesystem.
//
// `base_fd` is the first file descriptor memfs may return from open(). The
// caller owns the meaning of that value: the BSP passes 3 to leave the POSIX
// standard fds 0/1/2 (stdin/stdout/stderr) to the console, but memfs itself
// imposes no policy — it simply starts allocating fds at `base_fd`. The base is
// fixed for the lifetime of any open fd, and every later open() must therefore
// observe the same base (an fd issued with one base cannot be re-read after a
// re-init with a different base).
//
// `memfs_init()` reads the payload that the linker embedded via
// --tcm-memfs=<dir> (weak symbols memfs_image / memfs_image_size). This is the
// normal BSP entry point; it hides the payload ABI from application code.
//
// `memfs_init_with_payload()` allows tests and tooling to mount an in-memory
// payload directly.
//
// If no payload is present (or payload is NULL/size 0) the filesystem is
// created as an EMPTY directory: packed files do not resolve, but runtime file
// operations still work.
//
// Returns 0 on success, -1 on an invalid (recognised but corrupt) payload.
int memfs_init(int base_fd);
int memfs_init_with_payload(const void *payload, size_t size, int base_fd);

// POSIX flavours backed by memfs. `flags` follows <fcntl.h> conventions.
// All path arguments may be absolute ("/dir/file") or relative to the current
// working directory; "." and ".." are resolved by memfs.
int memfs_open_mode(const char *path, int flags, unsigned int mode);
int memfs_open_base(const char *path, int flags); // mode = 0644
int memfs_close(int fd);
int memfs_read(int fd, void *buf, size_t nbyte);
int memfs_write(int fd, const void *buf, size_t nbyte);
long memfs_lseek(int fd, long offset, int whence);
int memfs_stat(int fd, void *statbuf); // fstat-style, fills caller struct

// stat(path)-style: resolve `path` WITHOUT opening it. Fills the caller's struct
// stat with size + mode (regular file or directory). Returns 0 on success, or a
// negative errno (-ENOENT) when the name does not exist.
int memfs_stat_path(const char *path, void *statbuf);

// Map a regular file's content into memory and return a pointer to it WITHOUT
// opening it or copying it out. Whenever the file content happens to live in
// one contiguous memory region (e.g. a linker packed/image-burned asset), the
// returned pointer addresses exactly the file bytes and, if `out_len` is
// non-NULL, *`out_len` is filled with their length — suitable for zero-copy
// fast access. Returns NULL if the path does not resolve to a regular file, or
// if the content is not contiguous (spanned across runtime Chunk nodes), in
// which case no single pointer describes it. The region is normal writable RAM
// but is shared across consumers, so callers should treat it as read-only.
const void *memfs_map(const char *path, size_t *out_len);

// lstat(path)-style: like stat_path but does NOT follow the final symbolic link.
int memfs_lstat_path(const char *path, void *statbuf);

// unlink(path)-style: remove `path` from the directory. A file with open handles
// keeps its content until the last one closes (POSIX unlink-of-open-file).
// Returns 0 on success, or a negative errno (-ENOENT) when absent.
int memfs_unlink(const char *path);

// Directory enumeration. memfs maintains real directory nodes; opendir() returns
// the children of the directory at `path`. `path` of NULL / "" / "." means the
// root. Handles are opaque (cast to DIR* by the BSP).
void *memfs_opendir(const char *path);   // NULL on failure
int memfs_readdir(void *dir, void *ent); // 0 fills *ent; -ENOENT when exhausted
int memfs_closedir(void *dir);

// Create a directory at `path`. Intermediate path components must already exist.
// `mode` is stored for stat() but not enforced (no uid/gid/umask model).
// Returns 0 on success, or a negative errno:
//   -EEXIST  a node (file or directory) already exists at this path
//   -ENOENT  a parent component does not exist
//   -ENOTDIR a parent component is not a directory
//   -ENOSPC  / -ENOMEM on table / heap exhaustion
int memfs_mkdir_mode(const char *path, unsigned int mode);
int memfs_mkdir(const char *path); // convenience, mode = 0755

// Remove an empty directory at `path`.
// Returns 0 on success, or a negative errno:
//   -ENOENT      path does not exist
//   -ENOTDIR     path is not a directory
//   -ENOTEMPTY   directory is not empty
//   -EINVAL      attempt to remove "." or ".."
int memfs_rmdir(const char *path);

// Change the current working directory of the process. `path` may be absolute
// or relative, and is resolved through the current cwd. Returns 0 on success,
// or a negative errno:
//   -ENOENT      the target directory does not exist (or intermediate component)
//   -ENOTDIR     a parent component is not a directory
int memfs_chdir(const char *path);

// Copy the current working directory into `buf` (at most `size` bytes,
// including the NUL terminator). Returns `buf` on success, NULL on failure with
// errno set: ERANGE if the path is longer than `size`, EINVAL if `buf` is NULL.
char *memfs_getcwd(char *buf, size_t size);

// Rename `oldpath` to `newpath`. `newpath` may replace an existing file if it
// is not a directory. Intermediate directories in `newpath` must exist.
int memfs_rename(const char *oldpath, const char *newpath);

// Truncate a file by path or by fd. Lengths beyond the current size extend the
// file with zero bytes.
int memfs_truncate(const char *path, long length);
int memfs_ftruncate(int fd, long length);

// Change mode bits. memfs does not enforce permissions, so these are no-ops
// that still report success (the mode is stored for stat()).
int memfs_chmod(const char *path, unsigned int mode);
int memfs_fchmod(int fd, unsigned int mode);
// `flags` accepts AT_SYMLINK_NOFOLLOW; changing a symlink's own mode returns
// -EOPNOTSUPP (matches POSIX / Linux behaviour with fchmodat).
int memfs_fchmodat(const char *path, unsigned int mode, int flags);

// Symlink support. `memfs_symlink` creates a symbolic link at `linkpath`
// pointing to `target`; `memfs_readlink` reads the target into `buf`. memfs
// path resolution follows symlinks for normal lookups.
int memfs_symlink(const char *target, const char *linkpath);
ssize_t memfs_readlink(const char *path, char *buf, size_t bufsiz);

// Return a resolved absolute path. `resolved_path` may be NULL to request a
// malloc'd buffer. memfs has no real symlinks beyond those it creates, so this
// simply normalizes the path.
char *memfs_realpath(const char *path, char *resolved_path);

// Directory file descriptors for openat / fdopendir / dirfd / unlinkat.
// These are synthetic fds that do not overlap with regular file fds.
int memfs_dirfd(void *dir);                 // -errno on failure
void *memfs_dir_from_fd(int dirfd);         // NULL if fd is not a dir fd
int memfs_dirpath(void *dir, char *buf, size_t sz); // absolute path of a dir
int memfs_openat_dir(int dirfd, const char *name);  // returns synthetic dir fd
int memfs_unlinkat(int dirfd, const char *name, int flags);

// pathconf() values for memfs.
long memfs_pathconf(const char *path, int name);

// Debugging helpers (only available when TCM_MEMFS_DEBUG is defined).
// `memfs_debug_dump_tree()` prints from the root;
// `memfs_debug_dump_tree_at(path)` prints the subtree rooted at `path`
// ("" or "/" means the root).
#ifdef TCM_MEMFS_DEBUG
void memfs_debug_dump_tree(void);
void memfs_debug_dump_tree_at(const char *path);

// Runtime heap accounting.  Returns the number of bytes currently allocated
// through memfs' internal malloc paths and the number of live file-content
// chunks respectively.
size_t memfs_debug_heap_bytes(void);
size_t memfs_debug_chunk_count(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // MEMFS_H
