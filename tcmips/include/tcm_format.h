//
// tcm_format.h — Single source of truth for the TCM firmware image header
//                layout.  Toolchain copies under toolchains/ must be kept
//                in sync with this file.
//
// Linker augmentation: inst_np3 / inst_np4 added for the MIPS boot-stub
// instruction stub (6×uint32_t = 24 B total).  The original runtime-oriented
// header had only 4×uint32_t (16 B).  Three extra metadata fields
// (w_textbase, w_database, w_bssbase) were also added later (7 fields = 28 B
// vs the original 4 fields = 16 B).  Both changes are absorbed by shrinking
// vram[] so the packed size stays exactly TCM_HEADER_SIZE (4096 B).
//
// Any further changes here must be reflected in the toolchain sysroot copies
// and in the bootloader/boot ROM.
//
// Layout at image offset 0x00000000 (4 KB header zone):
//   +0      Inst stub (24 B, 6×uint32_t)    — inst_nop/np1/jmp/np2/np3/np4
//   +24     VRAM logo data (3804 B)          — uint8_t vram[96*40-36]
//   +3828   Metadata (28 B, 7×uint32_t)      — w_textbase/w_database/w_bssbase/
//                                           |   w_imagebase/w_imagesize/
//                                           |   w_timestamp/w_magic
//   +3856   filename (32 B)                  — char filename[0x20]
//   +3888   compiler (208 B)                 — char compiler[0xD0]
//   Total   24 + 3804 + 28 + 32 + 208 = 4096 B == TCM_HEADER_SIZE
//
// App images always start at 0x00001000.

#ifndef TCMIPS_DEVKIT_FORMAT_H
#define TCMIPS_DEVKIT_FORMAT_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#define _TCM_CAT(a, b) a##b
#define TCM_CAT(a, b) _TCM_CAT(a, b)
#define _TCM_STR(x) #x
#define TCM_STR(x) _TCM_STR(x)

#if defined(__cplusplus)
#define TCM_STATIC_ASSERT(e, msg) static_assert(e, msg)
#else
#define TCM_STATIC_ASSERT(e, msg) _Static_assert(e, msg)
#endif

#define TCM_PACKED __attribute__((packed))

// Runtime reserved symbol prefix
#define TCM_PREFIX __tcm_
#define TCM_SYMBOL_START TCM_CAT(TCM_PREFIX, start)
#define TCM_SYMBOL_CTORS TCM_CAT(TCM_PREFIX, ctors)
#define TCM_SYMBOL_DTORS TCM_CAT(TCM_PREFIX, dtors)

#define TCM_NAME_START TCM_STR(TCM_SYMBOL_START)
#define TCM_NAME_CTORS TCM_STR(TCM_SYMBOL_CTORS)
#define TCM_NAME_DTORS TCM_STR(TCM_SYMBOL_DTORS)

// TCM firmware header size
#define TCM_HEADER_SIZE 0x1000

// TCM firmware magic signature
#define TCM_MAGIC 0x62C63394

// TCM custom executable header, fixed 4 KB (TCM_HEADER_SIZE)
typedef struct {
  // Boot entry instruction stub — linker emits 6×uint32_t (24 B);
  // the original tcmips-devkit header had 4×uint32_t (16 B) without np3/np4.
  uint32_t inst_nop; // MIPS: nop
  uint32_t inst_np1; // MIPS: nop
  uint32_t inst_jmp; // MIPS: j <start>
  uint32_t inst_np2; // MIPS: nop (delay slot of j)

  // Linker augmentation: extra delay-slot NOPs.
  // These must reside before the VRAM logo block so that the overall header
  // layout remains 4 KB packed.
  uint32_t inst_np3; // MIPS: nop (delay slot, linker-added)
  uint32_t inst_np4; // MIPS: nop (delay slot, linker-added)

  // Embedded logo framebuffer (96×40 monochrome 1-bit per pixel, 3804 B).
  uint8_t vram[96 * 40 - 36];

  uint32_t w_textbase;
  uint32_t w_database;
  uint32_t w_bssbase;
  uint32_t w_imagebase; // Image load base address
  uint32_t w_imagesize; // Total image size
  uint32_t w_timestamp; // Build timestamp
  uint32_t w_magic;     // TCM format magic number

  char filename[0x20]; // Source file name
  char compiler[0xD0]; // Compiler info
} TCM_PACKED TCMImageHeader;

// Ensure fixed header size — sizeof(TCMImageHeader) must equal TCM_HEADER_SIZE
// (4096 B).  The linker version extends the inst stub from 4×uint32_t (16 B)
// to 6×uint32_t (24 B) with inst_np3/inst_np4, and also adds three metadata
// fields (w_textbase / w_database / w_bssbase) growing metadata from 4×uint32_t
// (16 B) to 7×uint32_t (28 B).  vram[] absorbs both increases by shrinking from
// 96*40-16 (3824 B) to 96*40-36 (3804 B):
//   inst(24) + vram(3804) + meta(28) + fname(32) + comp(208) = 4096.
TCM_STATIC_ASSERT(sizeof(TCMImageHeader) == TCM_HEADER_SIZE,
                  "TCMImageHeader size assertion failed");

#endif // TCMIPSFORMAT_H