# CMakeLists.txt for the TCMIPS stb library port.
#
# Original upstream repository:
#   https://github.com/nothings/stb
# Exact upstream HEAD commit at the time of this import:
#   2c980bb59875b0d32144a71867fbdebb2f77cd20
#   Author: Sean Barrett <nothings@rbdme.com>
#   Date:   2025-02-11
# Clone command used:
#   git clone --single-branch https://github.com/nothings/stb.git
# To reproduce this exact baseline:
#   cd stb && git checkout 2c980bb59875b0d32144a71867fbdebb2f77cd20
#
# License: Public Domain (http://nothings.org/public_domain/).
#   All headers are freely usable with no restriction.
#
# Scope (ported files, all pure-computation, no external-device dependencies):
#   - stb_image.h     (v2.30) — image decode (PNG/JPEG/BMP/GIF/TGA/HDR/...)
#   - stb_image_resize2.h (v2.18) — image resize (bilinear, etc.)
#   - stb_truetype.h  (v1.26) — TrueType glyph rasterization
#   - stb_ds.h        (v0.67) — dynamic arrays & hash tables
#   - stb_sprintf.h   (v1.10) — fast snprintf
#   - stb_rect_pack.h (v1.01) — rectangle packing (atlas layout)
#
# Excluded (not ported, see vendor notes for rationale):
#   - stb_vorbis.c, stb_leakcheck.h, stb_tilemap_editor.h (imgui),
#     stb_voxel_render.h (GL), stb_c_lexer.h, stb_herringbone_wang_tile.h
#
# Integration: included via add_subdirectory() from tcmips/lib/CMakeLists.txt.
#   Dependencies: none per-header (all six libraries are self-contained),
#   except stb_image which uses picolibc stdio via target c (for FILE* based
#   stbi_load). stb_image has its own internal zlib/deflate decoder (no
#   external zlib needed). stb_truetype and stb_image_resize2 use no external
#   resources.
#
# NUANCE: stb_truetype.h and stb_rect_pack.h both independently define
# struct stbrp_node / struct stbrp_context with different field sets.  If both
# IMPLEMENTATION macros are active in the same translation unit, the compiler
# reports "no member named 'next' in 'stbrp_node'".  For this reason each
# implementation that touches those types lives in its own .c file:
#
#   stb_image.c        — STB_IMAGE
#   stb_image_resize2.c — STB_IMAGE_RESIZE
#   stb_truetype.c     — STB_TRUETYPE (uses its own stbrp_* types)
#   stb_rect_pack.c    — STB_RECT_PACK (conflicts with above)
#   stb_ds.c           — STB_DS
#   stb_sprintf.c      — STB_SPRINTF
#
# Public header path: <stb/stb_*.h> (synced to sysroot via
# tcm_sync_headers_to_sysroot).

add_library(tcmips_stb STATIC
    src/stb_image.c
    src/stb_image_resize2.c
    src/stb_truetype.c
    src/stb_rect_pack.c
    src/stb_ds.c
    src/stb_sprintf.c
)

target_include_directories(tcmips_stb
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_link_libraries(tcmips_stb
    PUBLIC c  # stb_image: stbi_load() uses FILE* via picolibc stdio → BSP → memfs
              # stb_truetype: uses malloc/free/string from picolibc
              # stb_sprintf: uses memcpy/memset from picolibc
              # stb_ds: uses malloc/realloc/free from picolibc
)

# stb_* headers are heavily documented; suppress docs-as-code warnings.
target_compile_options(tcmips_stb PRIVATE -Wno-unused-function)

# Sync public headers (include/stb/*.h) into sysroot → usr/include/stb/*.h
# so user programs can #include <stb/stb_image.h>.
tcm_sync_headers_to_sysroot(tcmips_stb "${CMAKE_CURRENT_LIST_DIR}/include/")
