/* stb_image_resize2 — single implementation TU.
 * Includes STBIR_NO_SIMD to avoid emitting inline asm and vector type constructs
 * that the TCMIPS llvm-link-tcmips linker cannot translate.
 */

#define STBIR_NO_SIMD

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
