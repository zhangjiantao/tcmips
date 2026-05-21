//
// Created by zhangjiantao on 26-4-28.
//

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

// TCM firmware magic signature
#define TCM_MAGIC 0x62C63394

// TCM custom executable header, fixed 256 bytes
typedef struct {
  // Boot entry instruction stub
  uint32_t inst_nop; // MIPS: nop
  uint32_t inst_np1; // MIPS: nop
  uint32_t inst_jal; // MIPS: jal to entry
  uint32_t inst_np2; // MIPS: nop (delay slot)
  uint32_t inst_brk; // MIPS: break (debug trap)
  uint32_t inst_np3; // MIPS: nop (delay slot)
  uint32_t inst_jmp; // MIPS: j (loop back to trap)
  uint32_t inst_np4; // MIPS: nop (delay slot)

  uint32_t w_imagebase; // Image load base address
  uint32_t w_imagesize; // Total image size
  uint32_t w_timestamp; // Build timestamp
  uint32_t w_magic;     // TCM format magic number

  char filename[0x20];         // Source file name
  char compiler[0x100 - 0x50]; // Compiler info
} TCM_PACKED TCMImageHeader;

// Ensure fixed header size
TCM_STATIC_ASSERT(sizeof(TCMImageHeader) == 0x100,
                  "TCMImageHeader size assertion failed");

#endif // TCMIPSFORMAT_H
