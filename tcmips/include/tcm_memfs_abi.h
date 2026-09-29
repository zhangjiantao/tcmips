//// libmemfs - On-disk (in-image) catalog ABI shared with the linker.
//
// This header is the single source of truth for the binary layout the linker
// (llvm-link-tcmips --tcm-memfs=<dir>) bakes into a .tcm image, and that the
// runtime (libmemfs) later interprets. It is deliberately minimal and C++-safe:
//   * ONLY the layout types (MemfsPayloadHeader, MemfsEntry), MEMFS_MAGIC,
//     MEMFS_VERSION and the runtime-visible limits live here - no runtime API,
//     no <stddef.h> drag, no extern "C" wrappers for functions.
//   * The linker includes this directly; the runtime gets the same types via
//     tcm_memfs.h (which includes this).
//   * Any change here must be kept in sync with the linker and with the
//   runtime.
//
// Layout produced by the linker (all offsets relative to payload start):
//   MemfsPayloadHeader (16 B)
//   MemfsEntry[count]          (fixed-width 20 B records)
//   name region                (variable-length filenames, NUL-free)
//   per-file segment0          (packed content, plain RAM on TCMIPS)
//
// Linker responsibilities (enforced or validated at image generation time):
//   * memfs_image / memfs_image_size point at the payload base/byte length.
//   * magic == MEMFS_MAGIC; version == MEMFS_VERSION.
//   * final name length (plus terminating NUL) must be <= MEMFS_NAME_MAX.
//   * cleaned absolute path length (plus terminating NUL) must be <=
//   MEMFS_PATH_MAX.
//   * count * MEMFS_ENTRY_SIZE + MEMFS_HEADER_SIZE <= data_off <= payload size.

#ifndef TCM_MEMFS_ABI_H
#define TCM_MEMFS_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
#define MEMFS_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define MEMFS_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
// Pre-C11 toolchains give up the compile-time check but keep compiling.
#define MEMFS_STATIC_ASSERT(cond, msg) ((void)0)
#endif

// Magic placed at the head of the payload so the runtime can validate the
// catalog before interpreting it. The linker emits this first.
#define MEMFS_MAGIC 0x4D454D46u /* "MEMF" */

// ABI version stored in MemfsPayloadHeader.version. Bumping this lets the
// runtime reject images produced by a newer linker when the binary contract
// has changed. The linker must set version == MEMFS_VERSION.
#define MEMFS_VERSION 0u

// Runtime path/name limits. The linker is responsible for rejecting any entry
// whose final name or full path exceeds these bounds, so that the runtime never
// receives an image it cannot represent. Both limits INCLUDE the terminating
// NUL: a name of N bytes must satisfy N + 1 <= MEMFS_NAME_MAX, and a cleaned
// absolute path of P bytes must satisfy P + 1 <= MEMFS_PATH_MAX.
#ifndef MEMFS_NAME_MAX
#define MEMFS_NAME_MAX 256
#endif
#ifndef MEMFS_PATH_MAX
#define MEMFS_PATH_MAX 512
#endif

// Fixed on-disk sizes. These are duplicated here (and checked below) so the
// linker and runtime both agree on the catalog layout.
#define MEMFS_HEADER_SIZE 16u
#define MEMFS_ENTRY_SIZE 20u

#define _MEMFS_STR(x) #x
#define MEMFS_STR(x) _TCM_STR(x)

#define MEMFS_IMAGE __memfs_image
#define MEMFS_IMAGE_STR MEMFS_STR(MEMFS_IMAGE)
#define MEMFS_IMAGE_SIZE __memfs_image_size
#define MEMFS_IMAGE_SIZE_STR MEMFS_STR(MEMFS_IMAGE_SIZE)

// Force the exact byte layout. The structs contain only uint32_t fields, but
// packing them prevents a future compiler from inserting unexpected padding.
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#pragma pack(push, 1)
#endif

// Payload header (16 bytes).
typedef struct {
  uint32_t magic;    // == MEMFS_MAGIC
  uint32_t count;    // number of MemfsEntry records that follow
  uint32_t version;  // == MEMFS_VERSION
  uint32_t data_off; // byte offset from payload start to first segment0
} MemfsPayloadHeader;

// Fixed-length catalog record (20 bytes).
typedef struct {
  uint32_t name_off;    // byte offset into payload of this filename
  uint32_t name_len;    // filename bytes (no embedded NUL, no trailing NUL)
  uint32_t seg0_offset; // byte offset into payload of this file's segment0
  uint32_t seg0_size; // segment0 bytes = initial logical size = fixed capacity
  uint32_t mode;      // POSIX-ish open mode bits (O_RDONLY/O_RDWR...)
} MemfsEntry;

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#pragma pack(pop)
#endif

// Compile-time ABI layout checks. If these fire, the compiler used by either
// the linker or the runtime disagrees with the documented 16 B / 20 B layout.
MEMFS_STATIC_ASSERT(sizeof(MemfsPayloadHeader) == MEMFS_HEADER_SIZE,
                    "MemfsPayloadHeader must be 16 bytes");
MEMFS_STATIC_ASSERT(sizeof(MemfsEntry) == MEMFS_ENTRY_SIZE,
                    "MemfsEntry must be 20 bytes");

// Sanity checks on the exposed limits. The path limit must be able to contain
// at least a single maximal name at root ("/" + name + NUL).
MEMFS_STATIC_ASSERT(MEMFS_NAME_MAX >= 2,
                    "name limit must be at least 1 byte + NUL");
MEMFS_STATIC_ASSERT(
    MEMFS_PATH_MAX >= MEMFS_NAME_MAX + 1,
    "path limit must be able to hold at least one maximal name");

#ifdef __cplusplus
}
#endif

#endif // TCM_MEMFS_ABI_H
