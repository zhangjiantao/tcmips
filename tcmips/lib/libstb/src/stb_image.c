/* stb_image — single implementation TU.
 *
 * NUANCE: STBI_NO_THREAD_LOCALS is required because `__thread` TLS variables
 * generate `rdhwr` MIPS instructions (User Local Register access) whose LLVM
 * IR lowering the TCMIPS llvm-link-tcmips linker cannot translate on
 * mips32r2. TCMIPS is a single-threaded simulator so disabling TLS is
 * semantically equivalent.
 *
 * NUANCE: STBI_NO_* disables image decoders to reduce code size. NOTE: these
 * are stripped only from this TU; stbi_load()/stbi_load_from_memory() will
 * still compile but will return NULL at runtime for the disabled formats
 * (JPEG/HDR/PSD/TGA/PIC/PNM). PNG/BMP/GIF remain available.
 *
 * NUANCE: STBI_NO_SIMD prevents inline asm and `__vector_size__` constructs
 * from x86 SIMD paths that llvm-link-tcmips cannot lower for MIPS32.
 */

#define STBI_NO_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
