/* stb_truetype — single implementation TU.
 * Isolated from stb_rect_pack.c because stb_truetype.h includes its own internal
 * rectangle-packing types (stbrp_node / stbrp_context) that conflict with those
 * in stb_rect_pack.h when compiled in the same translation unit.
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
