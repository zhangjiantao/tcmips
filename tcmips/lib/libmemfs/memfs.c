// libmemfs implementation - TCMIPS in-memory filesystem runtime.
//
// See tcm_memfs.h / tcm_memfs_abi.h for the catalog contract, memory model,
// and POSIX semantics.
//
// Design invariants:
//   * Files and directories form a real in-memory tree: each node has a parent,
//     a first-child pointer (for directories), and a sibling pointer.
//   * The packed catalog produced by the linker is imported as a tree during
//     memfs_init(). For example, "a/b/c.txt" creates directories a and b and
//     file c.txt.
//   * File content is shared across all open() handles of the same name
//     (POSIX file-description model). The packed segment0 RAM plus any
//     runtime-allocated Chunk chain form one shared MemfsNode.
//   * Each open() handle owns an independent offset and open mode.
//   * Writes inside segment0 modify the packed RAM in place; writes past the
//     end append fixed-size Chunk nodes to the node's shared chain.
//   * Existing file data is never realloc'd or relocated.
//
// POSIX-flavoured semantics enforced here:
//   * open() honours O_ACCMODE for read/write permission and checks O_TRUNC.
//   * Opening a name not already in the directory requires O_CREAT.
//   * stat()/fstat() fill st_size and st_mode (regular file or directory).
//

#include <tcm_memfs.h>

#include <dirent.h> // struct dirent / DT_* type tags for readdir()
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> // SEEK_SET / SEEK_CUR / SEEK_END
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Host-toolchain portability. TCMIPS picolibc provides these POSIX constants,
// but the Windows headers used by a MinGW host do not.
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_LNK
#define DT_LNK 10
#endif

#ifndef AT_FDCWD
#define AT_FDCWD (-2)
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x0002
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x0008
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x0010
#endif

#if defined(_DIRENT_HAVE_D_TYPE) || defined(__linux__) || defined(__APPLE__)
#define MEMFS_HAS_D_TYPE 1
#endif

// Fixed node size for the runtime Chunk append chain. This is an
// implementation-internal constant (not part of the public ABI): keeping node
// sizes fixed avoids heap fragmentation. The public header deliberately does
// NOT expose it.
#ifndef MEMFS_CHUNK_SIZE
#define MEMFS_CHUNK_SIZE 4096u
#endif

// Runtime-only maximum number of concurrently live nodes (files and
// directories). Also implementation-private; sized for the BSP (console 0/1/2)
// plus a handful of resource files and directories. Override with -D if needed.
#ifndef MEMFS_MAX_OPEN
#define MEMFS_MAX_OPEN 128
#endif

// Runtime-only maximum number of concurrent file descriptors backed by memfs.
#ifndef MEMFS_MAX_FD
#define MEMFS_MAX_FD 64
#endif

// Maximum number of concurrent directory streams. Each stream holds a pointer
// to the directory node and iterates over its children. This bound is
// implementation-private.
#ifndef MEMFS_DIR_MAX
#define MEMFS_DIR_MAX 32
#endif

// Base value for synthetic file descriptors returned by dirfd() / fdopendir().
// They do not overlap with regular file descriptors (which start at base_fd),
// while still fitting in an int.
#ifndef MEMFS_DIRFD_BASE
#define MEMFS_DIRFD_BASE 1000
#endif

/* ---- embedded-payload placeholders --------------------------------------- */

// Weak placeholders: overridden by llvm-link-tcmips when --tcm-memfs=<dir> is
// used; otherwise remain as 0/empty.
__attribute__((weak, used, noinline)) uint8_t *MEMFS_IMAGE(void) {
  return NULL;
}

__attribute__((weak, used, noinline)) uint32_t MEMFS_IMAGE_SIZE(void) {
  return 0;
}

/* ---- runtime objects ----------------------------------------------------- */

typedef struct MemfsChunk {
  uint8_t data[MEMFS_CHUNK_SIZE];
  struct MemfsChunk *next;
} MemfsChunk;

// A filesystem node: directory, regular file, or symbolic link.
typedef struct MemfsNode {
  struct MemfsNode *parent;
  struct MemfsNode *child;   // First child, if this node is a directory.
  struct MemfsNode *sibling; // Next sibling under the same parent.

  const MemfsEntry *entry; // Catalog record; NULL for runtime-created nodes.
  char *name;              // Owned copy of the last path segment.
  bool is_dir;             // True for directories.
  bool is_symlink;         // True when symlink_target is valid.
  bool deleted;            // Name removed; content stays alive while refs > 0.
  unsigned int mode;       // POSIX mode bits (stored, not enforced).
  unsigned refs;           // Open handles and live references.
  uint32_t nlink;          // Directory link count (root starts at 2).

  // Regular-file content (valid iff !is_dir && !is_symlink).
  uint8_t *seg0;
  uint32_t seg0_size;
  MemfsChunk *chunks;
  uint32_t logical_size;

  // Symlink target (valid iff is_symlink).
  char *symlink_target;
} MemfsNode;

// An open file DESCRIPTION (per-open state). Holds a reference to a node.
typedef struct MemfsFile {
  bool used;
  MemfsNode *node;
  uint32_t pos; // Independent offset for this handle.
  int accmode;  // O_ACCMODE bits: O_RDONLY / O_WRONLY / O_RDWR.
  int flags;    // Full open() flags (for O_APPEND etc.).
} MemfsFile;

typedef struct MemfsDirStream {
  DIR dir; // Public DIR header; must be the first member.
  bool used;
  MemfsNode *parent;     // Directory being enumerated.
  MemfsNode *next_child; // Next child to return.
  int stage;             // 0=., 1=.., 2=children, 3=done.
  int dirfd;             // Synthetic fd assigned by dirfd(); -1 if none.
} MemfsDirStream;

/* ---- module state -------------------------------------------------------- */

static bool s_initialized;
static MemfsPayloadHeader *s_payload;
static size_t s_payload_size;
static const MemfsEntry *s_entries;
static uint32_t s_entry_count;
static int s_fd_base;

static MemfsNode s_nodes[MEMFS_MAX_OPEN];
static MemfsNode s_root;
static MemfsNode *s_cwd;

static MemfsFile s_files[MEMFS_MAX_FD];
static MemfsDirStream s_dirs[MEMFS_DIR_MAX];
static MemfsDirStream
    *s_dirfd_map[MEMFS_DIR_MAX]; // ds pointer for synthetic dir fd

/* ---- debug accounting (TCM_MEMFS_DEBUG only) --------------------------------- */

#ifdef TCM_MEMFS_DEBUG
static size_t s_memfs_debug_heap_bytes;
static size_t s_memfs_debug_chunk_count;

static void memfs_debug_track_alloc(size_t n, bool is_chunk) {
  s_memfs_debug_heap_bytes += n;
  if (is_chunk)
    s_memfs_debug_chunk_count++;
}

static void memfs_debug_track_free(size_t n, bool is_chunk) {
  s_memfs_debug_heap_bytes -= n;
  if (is_chunk)
    s_memfs_debug_chunk_count--;
}

static void memfs_debug_free_name(MemfsNode *n) {
  if (n && n->name) {
    s_memfs_debug_heap_bytes -= strlen(n->name) + 1;
    free(n->name);
    n->name = NULL;
  }
}

static void memfs_debug_free_symlink_target(MemfsNode *n) {
  if (n && n->symlink_target) {
    s_memfs_debug_heap_bytes -= strlen(n->symlink_target) + 1;
    free(n->symlink_target);
    n->symlink_target = NULL;
  }
}
#endif

/* ---- utility ------------------------------------------------------------- */

static MemfsDirStream *dir_from_handle(void *dir) {
  return (MemfsDirStream *)((uint8_t *)dir - offsetof(MemfsDirStream, dir));
}

static bool is_dirfd(int fd) { return fd >= MEMFS_DIRFD_BASE; }
static MemfsDirStream *dir_stream_from_dirfd(int dirfd);

static int close_dir_stream(MemfsDirStream *ds) {
  if (!ds || !ds->used)
    return -EBADF;
  if (ds->dirfd >= 0) {
    int idx = ds->dirfd - MEMFS_DIRFD_BASE;
    if (idx >= 0 && idx < MEMFS_DIR_MAX)
      s_dirfd_map[idx] = NULL;
  }
  memset(ds, 0, sizeof(*ds));
  return 0;
}

static char *dup_str(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len + 1);
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_track_alloc(len + 1, false);
#endif
  return out;
}

// Validate that an entry's name region lies within the payload and the name
// region bounds (between the catalog and data_off).
static bool entry_name_in_payload(const MemfsEntry *e) {
  if (!s_payload)
    return false;
  if (e->name_len == 0)
    return true; // tombstone; no region to validate
  size_t header_size = sizeof(MemfsPayloadHeader);
  size_t catalog_bytes = (size_t)s_payload->count * sizeof(MemfsEntry);
  size_t name_region_start = header_size + catalog_bytes;
  if (e->name_off < name_region_start)
    return false;
  if ((size_t)e->name_off + e->name_len > s_payload->data_off)
    return false;
  return true;
}

static bool entry_seg0_in_payload(const MemfsEntry *e) {
  if (!s_payload)
    return false;
  if (e->seg0_offset < s_payload->data_off)
    return false;
  if ((size_t)e->seg0_offset + e->seg0_size > s_payload_size)
    return false;
  return true;
}

static const MemfsEntry *entry_lookup(const char *path) {
  if (!s_payload)
    return NULL;
  for (uint32_t i = 0; i < s_entry_count; ++i) {
    const MemfsEntry *e = &s_entries[i];
    if (e->name_len == 0)
      continue;
    if (e->name_len != strlen(path))
      continue;
    if (!entry_name_in_payload(e))
      continue;
    const char *n = (const char *)s_payload + e->name_off;
    if (memcmp(n, path, e->name_len) == 0)
      return e;
  }
  return NULL;
}

static void entry_tombstone(const MemfsEntry *e) {
  MemfsEntry *w = (MemfsEntry *)e;
  w->name_off = 0;
  w->name_len = 0;
}

/* ---- tree management ----------------------------------------------------- */

// Allocate a fresh node slot from the pool.
static MemfsNode *node_acquire(void) {
  for (unsigned i = 0; i < MEMFS_MAX_OPEN; ++i) {
    MemfsNode *n = &s_nodes[i];
    if (n->name == NULL && n->refs == 0 && (n == &s_root || !n->is_dir)) {
      // A stale node that is no longer linked has name==NULL. Clear it.
      memset(n, 0, sizeof(*n));
      n->refs = 1;
      return n;
    }
  }
  return NULL;
}

// Recycle an unlinked node whose content has been released. Directory slots
// can be reused once they are unlinked and have no open references.
static void node_release(MemfsNode *n) {
  if (!n || n == &s_root)
    return;
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_free_symlink_target(n);
#else
  free(n->symlink_target);
  n->symlink_target = NULL;
#endif
  n->name = NULL;
  n->refs = 0;
  n->deleted = false;
  n->is_dir = false;
  n->is_symlink = false;
}

// Return the first child of `dir` whose last segment is `name`.
static MemfsNode *dir_find_child(const MemfsNode *dir, const char *name) {
  for (MemfsNode *c = dir->child; c; c = c->sibling) {
    if (c->deleted)
      continue;
    if (c->name && strcmp(c->name, name) == 0)
      return c;
  }
  return NULL;
}

// Link `child` as a member of parent directory `dir`.
static void dir_link_child(MemfsNode *dir, MemfsNode *child) {
  child->parent = dir;
  child->sibling = dir->child;
  dir->child = child;
  if (dir != &s_root)
    dir->nlink++;
}

// Unlink `child` from its parent. Caller must ensure child is in parent's list.
static void dir_unlink_child(MemfsNode *dir, MemfsNode *child) {
  MemfsNode **pp = &dir->child;
  while (*pp) {
    if (*pp == child) {
      *pp = child->sibling;
      child->sibling = NULL;
      child->parent = NULL;
      if (dir != &s_root)
        dir->nlink--;
      return;
    }
    pp = &(*pp)->sibling;
  }
}

// Find or create a directory as a child of `parent` with last segment `name`.
static MemfsNode *ensure_dir_child(MemfsNode *parent, const char *name) {
  MemfsNode *d = dir_find_child(parent, name);
  if (d) {
    if (!d->is_dir)
      return NULL; // Name collision with a regular file.
    return d;
  }
  d = node_acquire();
  if (!d)
    return NULL;
  d->name = dup_str(name);
  if (!d->name) {
    node_release(d);
    return NULL;
  }
  d->is_dir = true;
  d->mode = 0755;
  d->nlink = 2; // self + parent reference
  dir_link_child(parent, d);
  return d;
}

/* ---- 32-bit offset arithmetic helpers ------------------------------------ */

static inline bool add_overflow_u32(uint32_t a, uint32_t b) {
  return a > UINT32_MAX - b;
}

/* ---- block access over {seg0 + chunk chain} ------------------------------ */

static int ensure_capacity(MemfsNode *n, uint32_t off, uint32_t need) {
  if (need == 0)
    return 0;
  if (add_overflow_u32(off, need))
    return -EINVAL;
  uint32_t hi = off + need;
  if (hi <= n->seg0_size)
    return 0;
  uint32_t beyond = hi - n->seg0_size;
  uint32_t chunks_needed = (beyond + MEMFS_CHUNK_SIZE - 1) / MEMFS_CHUNK_SIZE;
  uint32_t have = 0;
  MemfsChunk *last = NULL;
  for (MemfsChunk *c = n->chunks; c; c = c->next) {
    ++have;
    last = c;
  }
  while (have < chunks_needed) {
    MemfsChunk *c = (MemfsChunk *)malloc(sizeof(MemfsChunk));
    if (!c)
      return -ENOMEM;
    memset(c, 0, sizeof(*c));
#ifdef TCM_MEMFS_DEBUG
    memfs_debug_track_alloc(sizeof(MemfsChunk), true);
#endif
    if (last)
      last->next = c;
    else
      n->chunks = c;
    last = c;
    ++have;
  }
  return 0;
}

static int node_write(MemfsNode *n, uint32_t off, const void *buf, size_t len) {
  if (len == 0)
    return 0;
  int rc = ensure_capacity(n, off, (uint32_t)len);
  if (rc < 0)
    return rc;
  const uint8_t *src = (const uint8_t *)buf;
  size_t left = len;
  uint32_t pos = off;

  /* seg0 region: memcpy up to the packed end */
  if (pos < n->seg0_size) {
    uint32_t avail = n->seg0_size - pos;
    size_t copy = left < avail ? left : (size_t)avail;
    memcpy(n->seg0 + pos, src, copy);
    pos += (uint32_t)copy;
    src += copy;
    left -= copy;
  }

  /* chunk region: locate the starting chunk once, then memcpy blockwise */
  if (left > 0) {
    uint32_t chain_idx = pos - n->seg0_size; /* pos >= seg0_size here */
    uint32_t ci = chain_idx / MEMFS_CHUNK_SIZE;
    uint32_t within = chain_idx % MEMFS_CHUNK_SIZE;
    MemfsChunk *c = n->chunks;
    while (c && ci > 0) {
      c = c->next;
      --ci;
    }
    while (left > 0 && c) {
      uint32_t avail = MEMFS_CHUNK_SIZE - within;
      size_t copy = left < avail ? left : (size_t)avail;
      memcpy(c->data + within, src, copy);
      pos += (uint32_t)copy;
      within = 0;
      src += copy;
      left -= copy;
      c = c->next;
    }
  }

  uint32_t new_end = off + (uint32_t)len;
  if (new_end > n->logical_size)
    n->logical_size = new_end;
  return 0;
}

static size_t node_read(const MemfsNode *n, uint32_t off, void *buf,
                        size_t len) {
  uint8_t *dst = (uint8_t *)buf;
  size_t copied = 0;
  uint32_t pos = off;
  size_t left = len;

  /* seg0 region: memcpy up to the packed end, but never past logical_size
   (the file may have been truncated below seg0_size). The extra
   `pos < logical_size` guard rejects offsets in [logical_size, seg0_size)
   (past the shrunk logical EOF), where `logical_size - pos` would underflow. */
  if (pos < n->seg0_size && pos < n->logical_size) {
    uint32_t avail_phys = n->seg0_size - pos;
    uint32_t avail_log = n->logical_size - pos;
    uint32_t avail = avail_phys < avail_log ? avail_phys : avail_log;
    size_t copy = left < avail ? left : (size_t)avail;
    memcpy(dst, n->seg0 + pos, copy);
    pos += (uint32_t)copy;
    dst += copy;
    copied += copy;
    left -= copy;
  }

  /* chunk region: locate the starting chunk once, then memcpy blockwise,
   * never reading past logical_end. */
  if (left > 0 && pos < n->logical_size) {
    uint32_t chain_idx = pos - n->seg0_size; /* pos >= seg0_size here */
    uint32_t ci = chain_idx / MEMFS_CHUNK_SIZE;
    uint32_t within = chain_idx % MEMFS_CHUNK_SIZE;
    MemfsChunk *c = n->chunks;
    while (c && ci > 0) {
      c = c->next;
      --ci;
    }
    while (left > 0 && c && pos < n->logical_size) {
      uint32_t phys_avail = MEMFS_CHUNK_SIZE - within; /* bytes left in chunk */
      uint32_t log_avail = n->logical_size - pos;      /* bytes left in file */
      uint32_t take = phys_avail < log_avail ? phys_avail : log_avail;
      if (left < take)
        take = (uint32_t)left;
      if (take == 0)
        break; /* logical end reached */
      memcpy(dst, c->data + within, take);
      dst += take;
      copied += take;
      pos += take;
      left -= take;
      within = (within + take) % MEMFS_CHUNK_SIZE;
      if (within == 0)
        c = c->next; /* advance only when the current chunk is fully read */
    }
  }
  return copied;
}

static void node_free_content(MemfsNode *n) {
  MemfsChunk *c = n->chunks;
  while (c) {
    MemfsChunk *next = c->next;
#ifdef TCM_MEMFS_DEBUG
    memfs_debug_track_free(sizeof(MemfsChunk), true);
#endif
    free(c);
    c = next;
  }
  n->chunks = NULL;
}

/* ---- path normalization / lookup ----------------------------------------- */

static int build_cwd_path(char *out, size_t outsz);
static void fill_stat(const MemfsNode *n, struct stat *st);
static bool node_is_descendant(const MemfsNode *ancestor,
                               const MemfsNode *node);

// Normalize a path into an absolute, cleaned path without "." or "..".
// Returns 0 on success, negative errno on failure.
// Input may already be absolute or relative.
static int normalize_path(const char *path, char *out, size_t outsz) {
  if (!path || !out || outsz < 2)
    return -EINVAL;

  char combined[MEMFS_PATH_MAX];
  const char *base;
  if (path[0] == '/') {
    base = path;
  } else if (!s_cwd) {
    // No cwd set; treat as relative to root.
    base = path;
  } else {
    // Build absolute path as "<cwd>/<path>".
    int rc = build_cwd_path(combined, sizeof(combined));
    if (rc < 0)
      return rc;
    size_t cwd_len = strlen(combined);
    size_t path_len = strlen(path);
    if (cwd_len + 1 + path_len + 1 > sizeof(combined))
      return -ENAMETOOLONG;
    if (cwd_len == 1 && combined[0] == '/') {
      combined[1] = '\0';
      strncat(combined, path, sizeof(combined) - 2);
    } else {
      combined[cwd_len] = '/';
      memcpy(combined + cwd_len + 1, path, path_len + 1);
    }
    base = combined;
  }

  // Now resolve base into an absolute cleaned path.
  if (base[0] != '/') {
    if (outsz < strlen(base) + 2)
      return -ENAMETOOLONG;
    strcpy(out, "/");
    strcat(out, base);
  } else {
    if (outsz < strlen(base) + 1)
      return -ENAMETOOLONG;
    strcpy(out, base);
  }

  // Remove duplicate and . segments.
  char tmp[MEMFS_PATH_MAX];
  char *t = tmp;
  const char *p = out;
  *t++ = '/';
  p++;
  while (*p) {
    if (*p == '/') {
      ++p;
      continue;
    }
    const char *start = p;
    while (*p && *p != '/')
      ++p;
    size_t seglen = (size_t)(p - start);
    if (seglen == 1 && start[0] == '.') {
      // ignore .
    } else if (seglen == 2 && start[0] == '.' && start[1] == '.') {
      // remove parent segment if present
      if (t > tmp + 1) {
        --t; // trailing '/'
        while (t > tmp + 1 && t[-1] != '/')
          --t;
      }
    } else {
      for (size_t i = 0; i < seglen; ++i)
        *t++ = start[i];
      *t++ = '/';
    }
  }
  // Remove trailing slash except for root.
  if (t > tmp + 1 && t[-1] == '/')
    --t;
  *t = '\0';

  if (strlen(tmp) + 1 > outsz)
    return -ENAMETOOLONG;
  strcpy(out, tmp);
  return 0;
}

// Resolve an absolute cleaned path to a node. Optionally create missing
// directory components (for importing the packed catalog) and/or resolve to
// the parent of the last segment.
//   create_dirs: if true, missing directory components are created (init only).
//   parent_out: if non-NULL, returns the parent directory and sets *name_out to
//               the last segment pointer into path (no allocation).
//   follow_final_symlink: if true, the final component is dereferenced if it is
//               a symlink (normal lookups); lstat/readlink use false.
// Returns pointer to node, or NULL on error and *err negative errno.
static MemfsNode *follow_symlink(MemfsNode *link, int depth, int *err);

static MemfsNode *resolve_node_ex_depth(const char *path, bool create_dirs,
                                        MemfsNode **parent_out,
                                        const char **name_out, int *err,
                                        bool follow_final_symlink, int depth);

static MemfsNode *resolve_node_ex(const char *path, bool create_dirs,
                                  MemfsNode **parent_out, const char **name_out,
                                  int *err, bool follow_final_symlink) {
  return resolve_node_ex_depth(path, create_dirs, parent_out, name_out, err,
                               follow_final_symlink, 0);
}

static MemfsNode *resolve_node_ex_depth(const char *path, bool create_dirs,
                                        MemfsNode **parent_out,
                                        const char **name_out, int *err,
                                        bool follow_final_symlink, int depth) {
  if (!path || path[0] != '/') {
    if (err)
      *err = -EINVAL;
    return NULL;
  }
  if (path[1] == '\0') {
    if (parent_out)
      *parent_out = NULL;
    if (name_out)
      *name_out = "";
    if (err)
      *err = 0;
    return &s_root;
  }

  MemfsNode *cur = &s_root;
  const char *p = path + 1;
  char comp[MEMFS_NAME_MAX];

  while (*p) {
    const char *end = strchr(p, '/');
    size_t len;
    if (!end) {
      len = strlen(p);
    } else {
      len = (size_t)(end - p);
      end++;
    }

    if (len == 0 || len >= sizeof(comp)) {
      if (parent_out)
        *parent_out = NULL;
      if (name_out)
        *name_out = NULL;
      if (err)
        *err = -EINVAL;
      return NULL;
    }
    memcpy(comp, p, len);
    comp[len] = '\0';

    MemfsNode *child = dir_find_child(cur, comp);
    if (!child || child->deleted) {
      if (end == NULL || end[-1] != '/') {
        // last component missing
        if (parent_out && !end) {
          *parent_out = cur;
          if (name_out)
            *name_out = p;
          if (err)
            *err = -ENOENT;
          return NULL;
        }
        if (err)
          *err = -ENOENT;
        return NULL;
      }
      if (create_dirs) {
        child = ensure_dir_child(cur, comp);
        if (!child) {
          if (err)
            *err = -ENOMEM;
          return NULL;
        }
      } else {
        if (err)
          *err = -ENOENT;
        return NULL;
      }
    }

    if (end == NULL) {
      if (follow_final_symlink && child->is_symlink) {
        child = follow_symlink(child, depth, err);
        if (!child)
          return NULL;
      }
      if (parent_out) {
        *parent_out = cur;
        if (name_out)
          *name_out = p;
      }
      if (err)
        *err = 0;
      return child;
    }

    if (child->is_symlink)
      child = follow_symlink(child, depth, err);
    if (!child)
      return NULL;
    if (!child->is_dir) {
      if (err)
        *err = -ENOTDIR;
      return NULL;
    }

    cur = child;
    p = end;
    if (depth >= 0)
      ++depth;
  }

  if (err)
    *err = 0;
  if (parent_out) {
    *parent_out = cur;
    if (name_out)
      *name_out = "";
  }
  return cur;
}

static MemfsNode *resolve_node(const char *path, int *err) {
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0) {
    if (err)
      *err = rc;
    return NULL;
  }
  return resolve_node_ex(norm, false, NULL, NULL, err, true);
}

static MemfsNode *resolve_node_no_follow(const char *path, int *err) {
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0) {
    if (err)
      *err = rc;
    return NULL;
  }
  return resolve_node_ex(norm, false, NULL, NULL, err, false);
}

// Follow a symbolic link up to a small recursion depth. Returns the target
// node, or NULL with *err set (negative errno).
static MemfsNode *follow_symlink(MemfsNode *link, int depth, int *err) {
  if (!link || !link->is_symlink)
    return link;
  if (depth > 8) {
    if (err)
      *err = -ELOOP;
    return NULL;
  }
  if (!link->symlink_target) {
    if (err)
      *err = -EIO;
    return NULL;
  }
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(link->symlink_target, norm, sizeof(norm));
  if (rc < 0) {
    if (err)
      *err = rc;
    return NULL;
  }
  MemfsNode *n =
      resolve_node_ex_depth(norm, false, NULL, NULL, err, true, depth + 1);
  if (!n)
    return NULL;
  // n itself could be a symlink if the target path resolves to one; continue.
  return follow_symlink(n, depth + 1, err);
}

// Search for an entry both in the live tree and in the packed catalog. Returns
// a live node if present, otherwise falls back to the catalog.
// Currently keep catalog as fallback for direct file creation from packed
// image; node tree is authoritative for runtime state.
//
// Note: after import the tree reflects both packed files and any dirs. Runtime
// files are created as nodes. resolve_node() only looks at the node tree.

/* ---- public API ---------------------------------------------------------- */

int memfs_init_with_payload(const void *payload, size_t size) {
  // Free all heap resources held by a previous incarnation before wiping the
  // node table.  This prevents memory leaks on repeated init and keeps the
  // TCM_MEMFS_DEBUG accounting counters accurate after reset.
  for (unsigned i = 0; i < MEMFS_MAX_OPEN; ++i) {
    MemfsNode *n = &s_nodes[i];
    free(n->name);
    node_free_content(n);
    free(n->symlink_target);
  }
  free(s_root.name);
  node_free_content(&s_root);
  free(s_root.symlink_target);
#ifdef TCM_MEMFS_DEBUG
  s_memfs_debug_heap_bytes = 0;
  s_memfs_debug_chunk_count = 0;
#endif

  memset(s_nodes, 0, sizeof(s_nodes));
  memset(&s_root, 0, sizeof(s_root));
  memset(s_files, 0, sizeof(s_files));
  memset(s_dirs, 0, sizeof(s_dirs));
  memset(s_dirfd_map, 0, sizeof(s_dirfd_map));
  s_fd_base = 3; // Reserve 0/1/2 for stdin/stdout/stderr

  s_root.name = dup_str("");
  s_root.is_dir = true;
  s_root.mode = 0755;
  s_root.nlink = 2;
  s_cwd = &s_root;

  if (!payload || size == 0) {
    s_payload = NULL;
    s_entries = NULL;
    s_entry_count = 0;
    s_payload_size = 0;
    s_initialized = true;
    return 0;
  }

  if (size < sizeof(MemfsPayloadHeader))
    return -1;

  MemfsPayloadHeader *h = (MemfsPayloadHeader *)payload;
  if (h->magic != MEMFS_MAGIC || h->version != MEMFS_VERSION) {
    s_initialized = false;
    return -1;
  }

  size_t catalog_bytes = (size_t)h->count * sizeof(MemfsEntry);
  size_t min_size = sizeof(MemfsPayloadHeader) + catalog_bytes;
  if (size < min_size || h->data_off < min_size) {
    s_initialized = false;
    return -1;
  }

  s_payload = h;
  s_payload_size = size;
  s_entry_count = h->count;
  s_entries = (const MemfsEntry *)((const uint8_t *)payload +
                                   sizeof(MemfsPayloadHeader));

  // Build the tree from the packed catalog. Packed paths use '/' as separator.
  for (uint32_t i = 0; i < s_entry_count; ++i) {
    const MemfsEntry *e = &s_entries[i];
    if (e->name_len == 0)
      continue;
    if (!entry_name_in_payload(e) || !entry_seg0_in_payload(e))
      continue;

    char raw[MEMFS_NAME_MAX];
    const char *src = (const char *)s_payload + e->name_off;
    if (e->name_len == 0 || e->name_len >= sizeof(raw))
      continue;
    memcpy(raw, src, e->name_len);
    raw[e->name_len] = '\0';

    // Packed catalog paths may be stored relative to root. resolve_node_ex
    // requires an absolute cleaned path, so prepend '/' when absent.
    char full[MEMFS_PATH_MAX];
    if (raw[0] == '/') {
      memcpy(full, raw, e->name_len + 1);
    } else {
      full[0] = '/';
      memcpy(full + 1, raw, e->name_len + 1);
    }

    MemfsNode *parent = NULL;
    const char *name = NULL;
    int err = 0;
    (void)resolve_node_ex(full, true, &parent, &name, &err, false);
    if (!parent || !name)
      continue;

    // If this packed record refers to a directory placeholder (path is an
    // existing directory), nothing more to do.
    MemfsNode *existing = dir_find_child(parent, name);
    if (existing && existing->is_dir) {
      existing->entry = e;
      continue;
    }

    MemfsNode *n = node_acquire();
    if (!n)
      continue;

    n->entry = e;
    n->name = dup_str(name);
    if (!n->name) {
      node_release(n);
      continue;
    }
    n->seg0 = (uint8_t *)s_payload + e->seg0_offset;
    n->seg0_size = e->seg0_size;
    n->logical_size = e->seg0_size;
    n->is_dir = false;
    n->refs = 1;
    n->mode = e->mode;
    dir_link_child(parent, n);
  }

  s_initialized = true;
  return 0;
}

int memfs_init(void) {
  // The weak payload symbols may be overridden by the linker at link time,
  // so read them at runtime instead of allowing the compiler to fold them.
  volatile uint32_t size = MEMFS_IMAGE_SIZE();
  if (size)
    return memfs_init_with_payload(MEMFS_IMAGE(), size);
  return memfs_init_with_payload(NULL, 0);
}

int memfs_mkdir_mode(const char *path, unsigned int mode) {
  if (!s_initialized || !path)
    return -EINVAL;
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0)
    return rc;

  MemfsNode *parent = NULL;
  const char *name = NULL;
  int err = 0;
  MemfsNode *n = resolve_node_ex(norm, false, &parent, &name, &err, false);
  if (n)
    return -EEXIST;
  if (!parent || !name)
    return err;
  if (!parent->is_dir)
    return -ENOTDIR;

  MemfsNode *dir = node_acquire();
  if (!dir)
    return -ENOMEM;

  // name points into norm; duplicate it.
  size_t nlen = strlen(name);
  if (nlen == 0) {
    memset(dir, 0, sizeof(*dir));
    return -EINVAL;
  }
  if (nlen >= MEMFS_NAME_MAX) {
    memset(dir, 0, sizeof(*dir));
    return -ENAMETOOLONG;
  }
  dir->name = (char *)malloc(nlen + 1);
  if (!dir->name) {
    memset(dir, 0, sizeof(*dir));
    return -ENOMEM;
  }
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_track_alloc(nlen + 1, false);
#endif
  memcpy(dir->name, name, nlen + 1);

  dir->is_dir = true;
  dir->mode = mode & 07777u;
  dir->nlink = 2;
  dir_link_child(parent, dir);
  return 0;
}

int memfs_mkdir(const char *path) { return memfs_mkdir_mode(path, 0755); }

int memfs_rmdir(const char *path) {
  if (!s_initialized || !path)
    return -EINVAL;
  // POSIX shells disallow removing the current directory itself.
  if (path[0] == '.' && path[1] == '\0')
    return -EINVAL;
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0)
    return rc;

  int err;
  MemfsNode *n = resolve_node_ex(norm, false, NULL, NULL, &err, false);
  if (!n)
    return err;
  if (!n->is_dir)
    return -ENOTDIR;
  if (n == &s_root)
    return -EINVAL;
  if (n == s_cwd)
    return -EBUSY;
  // A-only semantics: refuse to remove a directory if it is an ancestor of an
  // open directory stream. Removing the directory itself while it is open (B)
  // is allowed; the caller is responsible for closing that dangling stream.
  // This matches POSIX/Linux remove_all() behaviour.
  // This check is placed before the ENOTEMPTY check so that a stream-open
  // descendant always wins: even if the directory still has children, the
  // open-rooted subtree must not be pruned out from under the stream.
  for (uint32_t i = 0; i < MEMFS_DIR_MAX; ++i) {
    if (!s_dirs[i].used || !s_dirs[i].parent)
      continue;
    if (s_dirs[i].parent == n)
      continue; // allow self-delete of an open directory
    if (node_is_descendant(n, s_dirs[i].parent))
      return -EBUSY; // n is an ancestor of an open stream
  }
  if (n->child != NULL)
    return -ENOTEMPTY;

  dir_unlink_child(n->parent, n);
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_free_name(n);
#else
  free(n->name);
  n->name = NULL;
#endif
  n->deleted = true;
  n->refs = 0;
  node_release(n);
  return 0;
}


int memfs_chdir(const char *path) {
  if (!s_initialized)
    return -ENODEV;
  int err;
  MemfsNode *n = resolve_node(path, &err);
  if (!n)
    return err;
  if (!n->is_dir)
    return -ENOTDIR;
  s_cwd = n;
  return 0;
}

static int build_node_path(const MemfsNode *n, char *out, size_t outsz) {
  if (!out || outsz < 2)
    return -EINVAL;
  if (!n || n == &s_root) {
    if (outsz < 2)
      return -ERANGE;
    strcpy(out, "/");
    return 0;
  }

  char *p = out + outsz - 1;
  *p = '\0';
  const MemfsNode *d = n;
  while (d && d != &s_root && d->name) {
    size_t len = strlen(d->name);
    if ((size_t)(p - out) < len + 2)
      return -ERANGE;
    p -= len;
    memcpy(p, d->name, len);
    p -= 1;
    *p = '/';
    d = d->parent;
  }
  if (p == out + outsz - 1) {
    // Empty path (should be root).
    if (outsz < 2)
      return -ERANGE;
    strcpy(out, "/");
    return 0;
  }
  // p now points to leading '/'; shift to front.
  size_t len = (out + outsz - 1) - p;
  if (len >= outsz)
    return -ERANGE;
  memmove(out, p, len + 1);
  return 0;
}

static int build_cwd_path(char *out, size_t outsz) {
  return build_node_path(s_cwd, out, outsz);
}

char *memfs_getcwd(char *buf, size_t size) {
  static char internal_buf[MEMFS_PATH_MAX];
  char *out = buf;
  if (!out) {
    out = internal_buf;
    size = sizeof(internal_buf);
  }
  if (!out) {
    errno = EINVAL;
    return NULL;
  }
  int rc = build_cwd_path(out, size);
  if (rc < 0) {
    errno = -rc;
    return NULL;
  }
  return out;
}

int memfs_chmod(const char *path, unsigned int mode) {
  if (!s_initialized || !path)
    return -EINVAL;
  int err;
  MemfsNode *n = resolve_node(path, &err);
  if (!n)
    return err;
  n->mode = mode & 07777u;
  return 0;
}

int memfs_fchmod(int fd, unsigned int mode) {
  if (is_dirfd(fd)) {
    MemfsDirStream *ds = dir_stream_from_dirfd(fd);
    if (!ds)
      return -EBADF;
    ds->parent->mode = mode & 07777u;
    return 0;
  }
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  s_files[idx].node->mode = mode & 07777u;
  return 0;
}

int memfs_fchmodat(const char *path, unsigned int mode, int flags) {
  if (!s_initialized || !path)
    return -EINVAL;
  int err;
  MemfsNode *n;
  if (flags & AT_SYMLINK_NOFOLLOW)
    n = resolve_node_no_follow(path, &err);
  else
    n = resolve_node(path, &err);
  if (!n)
    return err;
  // POSIX does not allow changing the mode of a symbolic link itself.
  if ((flags & AT_SYMLINK_NOFOLLOW) && n->is_symlink)
    return -EOPNOTSUPP;
  n->mode = mode & 07777u;
  return 0;
}

/* ---- directory file descriptors (openat/fdopendir/dirfd/unlinkat) -------- */

// Allocate a synthetic fd for an existing directory stream.
static int alloc_dirfd(MemfsDirStream *ds) {
  if (!ds)
    return -EINVAL;
  if (ds->dirfd >= 0)
    return ds->dirfd;
  for (int i = 0; i < MEMFS_DIR_MAX; ++i) {
    if (s_dirfd_map[i] == NULL) {
      s_dirfd_map[i] = ds;
      ds->dirfd = MEMFS_DIRFD_BASE + i;
      return ds->dirfd;
    }
  }
  return -EMFILE;
}

static MemfsDirStream *dir_stream_from_dirfd(int dirfd) {
  int idx = dirfd - MEMFS_DIRFD_BASE;
  if (idx < 0 || idx >= MEMFS_DIR_MAX)
    return NULL;
  MemfsDirStream *ds = s_dirfd_map[idx];
  if (!ds || ds->dirfd != dirfd || !ds->used)
    return NULL;
  return ds;
}

int memfs_dirfd(void *dir) {
  if (!dir)
    return -EINVAL;
  MemfsDirStream *ds = dir_from_handle(dir);
  if (!ds->used || !ds->parent)
    return -EINVAL;
  return alloc_dirfd(ds);
}

void *memfs_dir_from_fd(int dirfd) {
  MemfsDirStream *ds = dir_stream_from_dirfd(dirfd);
  if (!ds)
    return NULL;
  return (void *)&ds->dir;
}

// Build an absolute path into `out` for a name relative to `parent`.
// `name` may be absolute, in which case `parent` is ignored.
static int build_child_path(const MemfsNode *parent, const char *name,
                            char *out, size_t outsz) {
  if (!parent || !name || !out || outsz < 4)
    return -EINVAL;
  if (name[0] == '/') {
    size_t nlen = strlen(name);
    if (nlen + 1 > outsz)
      return -ENAMETOOLONG;
    memcpy(out, name, nlen + 1);
    return 0;
  }
  char base[MEMFS_PATH_MAX];
  int rc = build_node_path(parent, base, sizeof(base));
  if (rc < 0)
    return rc;
  size_t blen = strlen(base);
  size_t nlen = strlen(name);
  if (blen + 1 + nlen + 1 > outsz)
    return -ENAMETOOLONG;
  if (blen == 1 && base[0] == '/') {
    out[0] = '/';
    memcpy(out + 1, name, nlen + 1);
  } else {
    memcpy(out, base, blen);
    out[blen] = '/';
    memcpy(out + blen + 1, name, nlen + 1);
  }
  return 0;
}

int memfs_dirpath(void *dir, char *buf, size_t sz) {
  if (!dir || !buf || sz < 2)
    return -EINVAL;
  MemfsDirStream *ds = dir_from_handle(dir);
  if (!ds->used || !ds->parent)
    return -EINVAL;
  return build_node_path(ds->parent, buf, sz);
}

int memfs_openat_dir(int dirfd, const char *name) {
  if (!s_initialized || !name)
    return -EINVAL;

  MemfsNode *parent;
  if (dirfd == AT_FDCWD) {
    parent = s_cwd;
  } else {
    MemfsDirStream *ds = dir_stream_from_dirfd(dirfd);
    if (!ds)
      return -EBADF;
    parent = ds->parent;
  }
  if (!parent)
    parent = &s_root;

  char path[MEMFS_PATH_MAX];
  int rc = build_child_path(parent, name, path, sizeof(path));
  if (rc < 0)
    return rc;

  int err;
  MemfsNode *n = resolve_node_ex(path, false, NULL, NULL, &err, true);
  if (!n)
    return err;
  if (!n->is_dir)
    return -ENOTDIR;

  MemfsDirStream *ds = NULL;
  for (uint32_t i = 0; i < MEMFS_DIR_MAX; ++i) {
    if (!s_dirs[i].used) {
      ds = &s_dirs[i];
      break;
    }
  }
  if (!ds)
    return -EMFILE;

  memset(ds, 0, sizeof(*ds));
  ds->used = true;
  ds->parent = n;
  ds->next_child = n->child;
  ds->stage = 0;
  ds->dirfd = -1;
  return alloc_dirfd(ds);
}

int memfs_unlinkat(int dirfd, const char *name, int flags) {
  if (!s_initialized || !name)
    return -EINVAL;

  MemfsNode *parent;
  if (dirfd == AT_FDCWD) {
    parent = s_cwd;
  } else {
    MemfsDirStream *ds = dir_stream_from_dirfd(dirfd);
    if (!ds)
      return -EBADF;
    parent = ds->parent;
  }
  if (!parent)
    parent = &s_root;

  char path[MEMFS_PATH_MAX];
  int rc = build_child_path(parent, name, path, sizeof(path));
  if (rc < 0)
    return rc;

  if (flags & AT_REMOVEDIR)
    return memfs_rmdir(path);
  return memfs_unlink(path);
}

// POSIX pathconf name values are standardized, but host/embedded headers may
// not expose them. Provide fallback values so memfs_pathconf works standalone.
#ifndef _PC_PATH_MAX
#define _PC_PATH_MAX 4
#endif
#ifndef _PC_NAME_MAX
#define _PC_NAME_MAX 3
#endif

long memfs_pathconf(const char *path, int name) {
  (void)path;
  if (name == _PC_PATH_MAX)
    return MEMFS_PATH_MAX;
  if (name == _PC_NAME_MAX)
    return MEMFS_NAME_MAX - 1;
  return -EINVAL;
}

static void node_truncate_chunks(MemfsNode *n, uint32_t new_size) {
  uint32_t needed_chunks = 0;
  if (new_size > n->seg0_size) {
    uint32_t beyond = new_size - n->seg0_size;
    needed_chunks = (beyond + MEMFS_CHUNK_SIZE - 1) / MEMFS_CHUNK_SIZE;
  }

  MemfsChunk *c = n->chunks;
  MemfsChunk *prev = NULL;
  uint32_t kept = 0;
  while (c) {
    if (kept < needed_chunks) {
      prev = c;
      c = c->next;
      ++kept;
    } else {
      MemfsChunk *victim = c;
      c = c->next;
      if (prev)
        prev->next = c;
      else
        n->chunks = c;
#ifdef TCM_MEMFS_DEBUG
      memfs_debug_track_free(sizeof(MemfsChunk), true);
#endif
      free(victim);
    }
  }
}

static int do_truncate(MemfsNode *n, long length) {
  if (!n || n->is_dir || n->is_symlink)
    return -EISDIR;
  if (length < 0 || (unsigned long)length > UINT32_MAX)
    return -EINVAL;
  uint32_t new_size = (uint32_t)length;
  if (new_size > n->logical_size) {
    // Extend with zeros. Write in small chunks to avoid a huge stack buffer.
    uint8_t zero[256];
    memset(zero, 0, sizeof(zero));
    for (uint32_t pos = n->logical_size; pos < new_size;) {
      uint32_t chunk = new_size - pos;
      if (chunk > sizeof(zero))
        chunk = sizeof(zero);
      int rc = node_write(n, pos, zero, chunk);
      if (rc < 0)
        return rc;
      pos += chunk;
    }
  }
  n->logical_size = new_size;
  node_truncate_chunks(n, new_size);
  return 0;
}

int memfs_truncate(const char *path, long length) {
  if (!s_initialized || !path)
    return -EINVAL;
  int err;
  MemfsNode *n = resolve_node(path, &err);
  if (!n)
    return err;
  return do_truncate(n, length);
}

int memfs_ftruncate(int fd, long length) {
  if (is_dirfd(fd))
    return -EINVAL;
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  if (s_files[idx].accmode == O_RDONLY)
    return -EBADF;
  return do_truncate(s_files[idx].node, length);
}

// Return true if `node` is `ancestor` or is located underneath it.
static bool node_is_descendant(const MemfsNode *ancestor,
                               const MemfsNode *node) {
  for (const MemfsNode *p = node; p; p = p->parent) {
    if (p == ancestor)
      return true;
  }
  return false;
}

int memfs_rename(const char *oldpath, const char *newpath) {
  if (!s_initialized || !oldpath || !newpath)
    return -EINVAL;
  char old_norm[MEMFS_PATH_MAX];
  char new_norm[MEMFS_PATH_MAX];
  if (normalize_path(oldpath, old_norm, sizeof(old_norm)) < 0)
    return -EINVAL;
  if (normalize_path(newpath, new_norm, sizeof(new_norm)) < 0)
    return -EINVAL;

  // Root cannot be renamed or replaced.
  if (old_norm[1] == '\0')
    return -EINVAL;

  MemfsNode *old_parent = NULL, *new_parent = NULL;
  const char *old_name = NULL, *new_name = NULL;
  int err;
  MemfsNode *oldn =
      resolve_node_ex(old_norm, false, &old_parent, &old_name, &err, false);
  if (!oldn)
    return err;
  if (oldn == &s_root)
    return -EINVAL;

  MemfsNode *newn =
      resolve_node_ex(new_norm, false, &new_parent, &new_name, &err, false);
  if (newn) {
    if (newn == oldn)
      return 0; // POSIX: renaming a path to itself is a no-op.
    if (newn == &s_root)
      return -EINVAL; // cannot replace root
  }

  if (!new_parent || !new_name)
    return err; // Intermediate component missing.

  // POSIX: cannot move a directory into itself or into one of its descendants.
  if (oldn->is_dir && node_is_descendant(oldn, new_parent))
    return -EINVAL;

  // A-only semantics: refuse to rename a directory if it is an ancestor of an
  // open directory stream. Renaming the directory itself while open is allowed.
  if (oldn->is_dir) {
    for (uint32_t i = 0; i < MEMFS_DIR_MAX; ++i) {
      if (!s_dirs[i].used || !s_dirs[i].parent)
        continue;
      if (s_dirs[i].parent == oldn)
        continue;
      if (node_is_descendant(oldn, s_dirs[i].parent))
        return -EBUSY;
    }
  }

  if (newn) {
    if (oldn->is_dir) {
      if (!newn->is_dir)
        return -ENOTDIR;
      if (newn->child != NULL)
        return -ENOTEMPTY;
      // A-only rule symmetric with rmdir: do not replace an empty directory
      // that is an ancestor of an open dir. Replacing it while open is allowed.
      for (uint32_t i = 0; i < MEMFS_DIR_MAX; ++i) {
        if (!s_dirs[i].used || !s_dirs[i].parent)
          continue;
        if (s_dirs[i].parent == newn)
          continue;
        if (node_is_descendant(newn, s_dirs[i].parent))
          return -EBUSY;
      }
      // Replace the empty directory: unlink from its parent and recycle the
      // node slot immediately (no open streams reference it at this point).
      dir_unlink_child(new_parent, newn);
#ifdef TCM_MEMFS_DEBUG
      memfs_debug_free_name(newn);
#else
      free(newn->name);
      newn->name = NULL;
#endif
      node_release(newn);
    } else {
      // oldn is a file or symlink; target must not be a directory.
      if (newn->is_dir)
        return -EISDIR;
      // Replace the existing file/symlink.
      newn->deleted = true;
      dir_unlink_child(new_parent, newn);
#ifdef TCM_MEMFS_DEBUG
      memfs_debug_free_name(newn);
      memfs_debug_free_symlink_target(newn);
#else
      free(newn->name);
      newn->name = NULL;
      free(newn->symlink_target);
      newn->symlink_target = NULL;
#endif
      if (newn->refs <= 1) {
        node_free_content(newn);
        node_release(newn);
      } else {
        // Still referenced by an open fd: keep the node slot until last close,
        // but release heap-allocated chunk content now (seg0 is shared packed
        // RAM and is left in place; the node is unreachable by name).
        node_free_content(newn);
      }
    }
  }

  // Unlink from old parent and relink under new parent with the new name.
  dir_unlink_child(old_parent, oldn);
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_free_name(oldn);
#else
  free(oldn->name);
#endif
  size_t nlen = strlen(new_name);
  oldn->name = (char *)malloc(nlen + 1);
  if (!oldn->name)
    return -ENOMEM;
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_track_alloc(nlen + 1, false);
#endif
  memcpy(oldn->name, new_name, nlen + 1);
  dir_link_child(new_parent, oldn);

  // The old packed catalog entry, if any, is no longer authoritative.
  const MemfsEntry *e = entry_lookup(old_norm);
  if (e)
    entry_tombstone(e);
  oldn->entry = NULL;
  return 0;
}

int memfs_symlink(const char *target, const char *linkpath) {
  if (!s_initialized || !target || !linkpath)
    return -EINVAL;
  char norm[MEMFS_PATH_MAX];
  if (normalize_path(linkpath, norm, sizeof(norm)) < 0)
    return -EINVAL;

  MemfsNode *parent = NULL;
  const char *name = NULL;
  int err;
  MemfsNode *n = resolve_node_ex(norm, false, &parent, &name, &err, false);
  if (n)
    return -EEXIST;
  if (!parent || !name)
    return err;

  n = node_acquire();
  if (!n)
    return -ENOMEM;

  size_t nlen = strlen(name);
  n->name = (char *)malloc(nlen + 1);
  if (!n->name) {
    node_release(n);
    return -ENOMEM;
  }
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_track_alloc(nlen + 1, false);
#endif
  memcpy(n->name, name, nlen + 1);

  size_t tlen = strlen(target);
  n->symlink_target = (char *)malloc(tlen + 1);
  if (!n->symlink_target) {
#ifdef TCM_MEMFS_DEBUG
    memfs_debug_free_name(n);
#else
    free(n->name);
    n->name = NULL;
#endif
    node_release(n);
    return -ENOMEM;
  }
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_track_alloc(tlen + 1, false);
#endif
  memcpy(n->symlink_target, target, tlen + 1);

  n->is_symlink = true;
  n->mode = 0777;
  n->refs = 1;
  dir_link_child(parent, n);
  return 0;
}

ssize_t memfs_readlink(const char *path, char *buf, size_t bufsiz) {
  // POSIX readlink: copies up to bufsiz bytes of the target and returns the
  // number of bytes written; it does NOT NUL-terminate.
  if (!s_initialized || !path || !buf)
    return -EINVAL;
  int err;
  MemfsNode *n = resolve_node_no_follow(path, &err);
  if (!n)
    return err;
  if (!n->is_symlink)
    return -EINVAL;
  if (!n->symlink_target)
    return -EIO;
  size_t len = strlen(n->symlink_target);
  size_t copy = len < bufsiz ? len : bufsiz;
  memcpy(buf, n->symlink_target, copy);
  return (ssize_t)copy;
}

char *memfs_realpath(const char *path, char *resolved_path) {
  // When resolved_path is NULL the caller expects a malloc'd buffer (libc++
  // canonical() frees the result).
  char *out = resolved_path ? resolved_path : (char *)malloc(MEMFS_PATH_MAX);
  if (!out) {
    errno = resolved_path ? EINVAL : ENOMEM;
    return NULL;
  }
  if (!path) {
    if (!resolved_path)
      free(out);
    errno = EINVAL;
    return NULL;
  }
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0) {
    if (!resolved_path)
      free(out);
    errno = -rc;
    return NULL;
  }
  int err;
  MemfsNode *n = resolve_node_ex(norm, false, NULL, NULL, &err, true);
  if (!n) {
    if (!resolved_path)
      free(out);
    errno = -err;
    return NULL;
  }
  if (build_node_path(n, out, MEMFS_PATH_MAX) < 0) {
    if (!resolved_path)
      free(out);
    errno = ERANGE;
    return NULL;
  }
  return out;
}

int memfs_lstat_path(const char *path, void *statbuf) {
  if (!s_initialized || !path || !statbuf)
    return -EINVAL;
  int err;
  MemfsNode *n = resolve_node_no_follow(path, &err);
  if (!n)
    return err;
  fill_stat(n, (struct stat *)statbuf);
  return 0;
}

int memfs_open_mode(const char *path, int flags, int base_fd,
                    unsigned int mode) {
  if (!s_initialized || !path || base_fd < 0)
    return -EINVAL;

  // All memfs regular-file fds are offset from a single base_fd fixed at init
  // time (default 3, leaving 0/1/2 for the BSP console). read/write/lseek/
  // close/fstat translate public fds back to internal indices by subtracting
  // s_fd_base, so the base must stay stable for the lifetime of any open fd.
  // Reject a caller using a different base_fd instead of silently invalidating
  // previously returned fds.
  if (base_fd != s_fd_base)
    return -EINVAL;

  int err;
  MemfsNode *parent = NULL;
  const char *name = NULL;
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0)
    return rc;

  bool follow_final_symlink = !(flags & O_NOFOLLOW);
  MemfsNode *n = resolve_node_ex(norm, false, &parent, &name, &err,
                                 follow_final_symlink);

  int accmode = flags & O_ACCMODE;
  bool created = false;

  if (!n) {
    if (err != -ENOENT || !(flags & O_CREAT) || !parent)
      return err;

    n = node_acquire();
    if (!n)
      return -ENOMEM;
    size_t nlen = strlen(name);
    if (nlen == 0 || nlen >= MEMFS_NAME_MAX) {
      node_release(n);
      return -EINVAL;
    }
    n->name = (char *)malloc(nlen + 1);
    if (!n->name) {
      node_release(n);
      return -ENOMEM;
    }
#ifdef TCM_MEMFS_DEBUG
    memfs_debug_track_alloc(nlen + 1, false);
#endif
    memcpy(n->name, name, nlen + 1);

    // Check for a catalog entry with the same name (tombstoned previously or
    // for run-time creation in the root's packed record). In general the
    // catalog entry may exist for a file created by the linker but removed;
    // allow reuse.
    const MemfsEntry *e = entry_lookup(norm);
    if (e && entry_seg0_in_payload(e)) {
      n->entry = e;
      n->seg0 = (uint8_t *)s_payload + e->seg0_offset;
      n->seg0_size = e->seg0_size;
      n->logical_size = e->seg0_size;
    } else {
      n->entry = NULL;
      n->seg0 = NULL;
      n->seg0_size = 0;
      n->logical_size = 0;
    }
    n->is_dir = false;
    n->mode = (mode & 07777u) | 0200u; // ensure owner-write for future writes
    n->refs = 1;
    dir_link_child(parent, n);
    created = true;
  }

  if ((flags & O_NOFOLLOW) && n && n->is_symlink)
    return -ELOOP;
  if ((flags & O_DIRECTORY) && (!n || !n->is_dir))
    return -ENOTDIR;
  if (n->is_dir || n->is_symlink)
    return -EISDIR;
  if ((flags & O_EXCL) && (flags & O_CREAT) && !created)
    return -EEXIST;

  if ((flags & O_TRUNC) && accmode != O_RDONLY && n->logical_size > 0) {
    n->logical_size = 0;
    node_free_content(n);
  }

  // Find a free file descriptor.
  int fd = -1;
  for (uint32_t i = 0; i < MEMFS_MAX_FD; ++i) {
    if (!s_files[i].used) {
      fd = i;
      break;
    }
  }
  if (fd < 0)
    return -ENFILE;

  s_files[fd].used = true;
  s_files[fd].node = n;
  s_files[fd].accmode = accmode;
  s_files[fd].flags = flags;
  // O_APPEND only affects writes; read offset must start at 0 (even for a
  // read-only handle opened with O_APPEND). memfs_write re-seeks to EOF at
  // write time based on the O_APPEND flag.
  s_files[fd].pos = 0;
  n->refs++;
  return fd + s_fd_base;
}

int memfs_open_base(const char *path, int flags, int base_fd) {
  return memfs_open_mode(path, flags, base_fd, 0644);
}

int memfs_close(int fd) {
  if (is_dirfd(fd)) {
    MemfsDirStream *ds = dir_stream_from_dirfd(fd);
    if (!ds)
      return -EBADF;
    return close_dir_stream(ds);
  }
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  MemfsFile *f = &s_files[idx];
  MemfsNode *n = f->node;
  f->used = false;
  f->node = NULL;
  if (n && n->refs > 0) {
    --n->refs;
    if (n->refs == 0 && n->deleted) {
      node_free_content(n);
      node_release(n);
    }
  }
  return 0;
}

int memfs_read(int fd, void *buf, size_t nbyte) {
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  MemfsFile *f = &s_files[idx];
  if (f->accmode == O_WRONLY)
    return -EBADF; /* POSIX: read() on a write-only fd -> EBADF */
  size_t got = node_read(f->node, f->pos, buf, nbyte);
  f->pos += (uint32_t)got;
  return (int)got;
}

int memfs_write(int fd, const void *buf, size_t nbyte) {
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  MemfsFile *f = &s_files[idx];
  if (f->accmode == O_RDONLY)
    return -EBADF; /* POSIX: write() on a read-only fd -> EBADF */
  if (f->flags & O_APPEND)
    f->pos = f->node->logical_size;
  if (nbyte > UINT32_MAX - f->pos)
    return -EINVAL;
  int rc = node_write(f->node, f->pos, buf, nbyte);
  if (rc < 0)
    return rc;
  f->pos += (uint32_t)nbyte;
  return (int)nbyte;
}

long memfs_lseek(int fd, long offset, int whence) {
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  MemfsFile *f = &s_files[idx];
  long size = (long)f->node->logical_size;
  long np;
  switch (whence) {
  case SEEK_SET:
    np = offset;
    break;
  case SEEK_CUR:
    np = (long)f->pos + offset;
    break;
  case SEEK_END:
    np = size + offset;
    break;
  default:
    return -EINVAL;
  }
  if (np < 0)
    return -EINVAL;
  // f->pos is uint32_t; compare in unsigned so the upper-bound check is
  // independent of `long` width (avoiding (long)UINT32_MAX == -1 on 32-bit).
  if ((unsigned long)np > UINT32_MAX)
    return -EINVAL;
  f->pos = (uint32_t)np;
  return (long)f->pos;
}

static void fill_stat(const MemfsNode *n, struct stat *st) {
  memset(st, 0, sizeof(*st));
  if (n->is_dir) {
    st->st_mode = S_IFDIR | (n->mode ? n->mode : 0755);
    st->st_size = 0;
  } else if (n->is_symlink) {
    st->st_mode = S_IFLNK | (n->mode ? n->mode : 0777);
    st->st_size = n->symlink_target ? (off_t)strlen(n->symlink_target) : 0;
  } else {
    st->st_mode = S_IFREG | (n->mode ? n->mode : 0644);
    st->st_size = (off_t)n->logical_size;
  }
  st->st_nlink = n->nlink ? n->nlink : 1;
  st->st_dev = 1; // Single in-memory filesystem.
  st->st_ino = (ino_t)(uintptr_t)n;
}

int memfs_stat(int fd, void *statbuf) {
  struct stat *st = (struct stat *)statbuf;
  if (!st)
    return -EINVAL;
  if (is_dirfd(fd)) {
    MemfsDirStream *ds = dir_stream_from_dirfd(fd);
    if (!ds)
      return -EBADF;
    fill_stat(ds->parent, st);
    return 0;
  }
  if (fd < s_fd_base)
    return -EBADF;
  int idx = fd - s_fd_base;
  if (idx < 0 || idx >= MEMFS_MAX_FD || !s_files[idx].used)
    return -EBADF;
  fill_stat(s_files[idx].node, st);
  return 0;
}

int memfs_stat_path(const char *path, void *statbuf) {
  if (!s_initialized || !path || !statbuf)
    return -EINVAL;
  int err;
  MemfsNode *n = resolve_node(path, &err);
  if (!n)
    return err;
  fill_stat(n, (struct stat *)statbuf);
  return 0;
}

const void *memfs_map(const char *path, size_t *out_len) {
  if (!s_initialized || !path)
    return NULL;
  int err;
  MemfsNode *n = resolve_node(path, &err);
  if (!n || n->is_dir || n->is_symlink)
    return NULL;

  // A file's content is contiguous whenever it lives entirely in its segment0
  // single RAM region and no runtime write appended non-contiguous Chunk nodes.
  // That is the case for every permanently backed/packed asset, and regardless
  // of origin the caller gets a direct zero-copy pointer over the whole bytes.
  if (n->chunks != NULL || n->seg0 == NULL)
    return NULL;
  if (out_len)
    *out_len = (size_t)n->logical_size;
  return (const void *)n->seg0;
}

int memfs_unlink(const char *path) {
  if (!s_initialized || !path)
    return -EINVAL;
  char norm[MEMFS_PATH_MAX];
  int rc = normalize_path(path, norm, sizeof(norm));
  if (rc < 0)
    return rc;

  MemfsNode *parent = NULL, *n;
  const char *name = NULL;
  int err;
  n = resolve_node_ex(norm, false, &parent, &name, &err, false);
  if (!n)
    return err;
  if (n->is_dir)
    return -EISDIR;

  // If a catalog entry backs this name, tombstone it as well.
  const MemfsEntry *e = entry_lookup(norm);
  if (e)
    entry_tombstone(e);

  n->deleted = true;
  dir_unlink_child(parent, n);
#ifdef TCM_MEMFS_DEBUG
  memfs_debug_free_name(n);
#else
  free(n->name);
  n->name = NULL;
#endif
  if (n->refs <= 1) {
    // No open handle holds this node: its name was the last reference.
    node_free_content(n);
    node_release(n);
  }
  return 0;
}

/* ---- directory enumeration (real directory tree) ------------------------- */

void *memfs_opendir(const char *path) {
  if (!s_initialized)
    return NULL;
  int err;
  // The public header documents NULL / "" / "." as aliases for the root.
  // resolve_node/normalize_path reject NULL, so map it to the root here.
  MemfsNode *n;
  if (!path || path[0] == '\0' || (path[0] == '.' && path[1] == '\0'))
    n = &s_root;
  else
    n = resolve_node(path, &err);
  if (!n)
    return NULL;
  if (!n->is_dir)
    return NULL;

  MemfsDirStream *ds = NULL;
  for (uint32_t i = 0; i < MEMFS_DIR_MAX; ++i) {
    if (!s_dirs[i].used) {
      ds = &s_dirs[i];
      break;
    }
  }
  if (!ds)
    return NULL;

  memset(ds, 0, sizeof(*ds));
  ds->used = true;
  ds->parent = n;
  ds->next_child = n->child;
  ds->stage = 0;
  ds->dirfd = -1;
  return (void *)&ds->dir;
}

int memfs_readdir(void *dir, void *ent) {
  if (!dir || !ent)
    return -1;
  MemfsDirStream *ds = dir_from_handle(dir);
  if (!ds->used || !ds->parent)
    return -1;

  struct dirent *de = (struct dirent *)ent;
  if (ds->stage == 0) {
    ds->stage = 1;
    de->d_name[0] = '.';
    de->d_name[1] = '\0';
#ifdef MEMFS_HAS_D_TYPE
    de->d_type = DT_DIR;
#endif
    de->d_ino = (ino_t)(uintptr_t)ds->parent;
    return 0;
  }
  if (ds->stage == 1) {
    ds->stage = 2;
    // '..' points at the parent directory, or the root when already at root.
    const MemfsNode *up = ds->parent->parent ? ds->parent->parent : ds->parent;
    de->d_name[0] = '.';
    de->d_name[1] = '.';
    de->d_name[2] = '\0';
#ifdef MEMFS_HAS_D_TYPE
    de->d_type = DT_DIR;
#endif
    de->d_ino = (ino_t)(uintptr_t)up;
    return 0;
  }
  while (ds->next_child) {
    MemfsNode *c = ds->next_child;
    ds->next_child = c->sibling;
    if (c->deleted)
      continue;
    if (!c->name)
      continue;
    size_t nlen = strlen(c->name);
    if (nlen >= sizeof(de->d_name))
      nlen = sizeof(de->d_name) - 1;
    memcpy(de->d_name, c->name, nlen);
    de->d_name[nlen] = '\0';
#ifdef MEMFS_HAS_D_TYPE
    if (c->is_symlink)
      de->d_type = DT_LNK;
    else if (c->is_dir)
      de->d_type = DT_DIR;
    else
      de->d_type = DT_REG;
#endif
    de->d_ino = (ino_t)(uintptr_t)c;
    return 0;
  }
  return -ENOENT;
}

int memfs_closedir(void *dir) {
  if (!dir)
    return -1;
  MemfsDirStream *ds = dir_from_handle(dir);
  if (!ds->used)
    return -1;
  return close_dir_stream(ds);
}

/* ---- debugging helpers (TCM_MEMFS_DEBUG only) -------------------------------- */

#ifdef TCM_MEMFS_DEBUG

static void dump_indent(int depth) {
  for (int i = 0; i < depth; ++i)
    printf("  ");
}

static void dump_node(const MemfsNode *n, int depth) {
  if (!n)
    return;
  const char *name = n->name ? n->name : "/";
  char type = n->is_dir ? 'd' : (n->is_symlink ? 'l' : 'f');

  dump_indent(depth);
  if (n->deleted) {
    printf("[%c] %s (deleted, refs=%u)\n", type, name, n->refs);
  } else if (n->is_dir) {
    printf("[%c] %s/ (mode=0%o, nlink=%u, refs=%u)\n", type, name, n->mode,
           n->nlink, n->refs);
  } else if (n->is_symlink) {
    printf("[%c] %s -> %s (mode=0%o, refs=%u)\n", type, name,
           n->symlink_target ? n->symlink_target : "?", n->mode, n->refs);
  } else {
    printf("[%c] %s (mode=0%o, size=%u, refs=%u)\n", type, name, n->mode,
           n->logical_size, n->refs);
  }

  for (MemfsNode *c = n->child; c; c = c->sibling)
    dump_node(c, depth + 1);
}

void memfs_debug_dump_tree(void) {
  if (!s_initialized) {
    printf("memfs not initialized\n");
    return;
  }
  printf("[memfs-debug] heap=%zu bytes, chunks=%zu\n",
         s_memfs_debug_heap_bytes, s_memfs_debug_chunk_count);
  dump_node(&s_root, 0);
}

void memfs_debug_dump_tree_at(const char *path) {
  if (!s_initialized) {
    printf("memfs not initialized\n");
    return;
  }
  printf("[memfs-debug] heap=%zu bytes, chunks=%zu\n",
         s_memfs_debug_heap_bytes, s_memfs_debug_chunk_count);
  MemfsNode *n = &s_root;
  if (path && path[0] != '\0' && !(path[0] == '/' && path[1] == '\0')) {
    int err = 0;
    n = resolve_node(path, &err);
    if (!n) {
      printf("memfs_debug_dump_tree_at: cannot resolve \"%s\" (err=%d)\n", path,
             err);
      return;
    }
  }
  dump_node(n, 0);
}

size_t memfs_debug_heap_bytes(void) { return s_memfs_debug_heap_bytes; }

size_t memfs_debug_chunk_count(void) { return s_memfs_debug_chunk_count; }

#endif
