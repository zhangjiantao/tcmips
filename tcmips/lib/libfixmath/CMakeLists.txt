# CMakeLists.txt for the TCMIPS libfixmath port.
#
# Original upstream repository:
#   https://github.com/PetteriAimonen/libfixmath
# Exact upstream HEAD commit at the time of this import:
#   ed9391c33a56078c126e3e7a811a883c8c87daa2 (2025-04-10 via shallow clone)
#   Message: "fix: avoid overflow in fix16_to_int"
# Clone command used:
#   git clone --depth 1 --single-branch \
#     https://github.com/PetteriAimonen/libfixmath.git
# To reproduce this exact baseline:
#   git clone https://github.com/PetteriAimonen/libfixmath.git libfixmath
#   cd libfixmath && git checkout ed9391c33a56078c126e3e7a811a883c8c87daa2
#
# Included sources (lightweight subset — no exp/log/atan2/fft/str):
#   - fix16.c  (mul/div/add/sub/mod/lerp — core arithmetic)
#   - fix16_sqrt.c  (square root, pure 32-bit integer algorithm)
#   - fix16_trig.c + fix16_trig_sin_lut.h  (sin/cos/tan/asin/acos/atan
#     using a compact lookup table + linear interpolation)
#   - int64.h  (64-bit arithmetic abstraction, header-only)
#
# Integration: included via add_subdirectory() from tcmips/lib/CMakeLists.txt.
#   Dependencies: c (picolibc: stdint, limits, stdlib), tcmips_dev (none yet;
#     kept for consistency with other ported libs).
#   Public header: fixmath/fix16.h (synced as <fixmath/fix16.h>).
#   Internal headers are NOT synced; the library's own source files find them
#   via target_include_directories (PRIVATE).
#
# License: MIT.  Copyright (c) 2011-2024 Ben Brewer, Petteri Aimonen, et al.
#   Full license text is vendored at LICENSE and must be preserved in any
#   redistribution (see the THIRD-PARTY NOTICES appendix of the root LICENSE).

add_library(tcmips_fixmath STATIC
    src/fix16.c
    src/fix16_exp.c
    src/fix16_fft.c
    src/fix16_sqrt.c
    src/fix16_str.c
    src/fix16_trig.c
    src/fract32.c
    src/uint32.c
)

target_include_directories(tcmips_fixmath
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/fixmath>  # fix16.h for <fixmath/fix16.h>
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>              # int64.h, fix16_trig_sin_lut.h
)

target_link_libraries(tcmips_fixmath
    PRIVATE c
)

target_compile_options(tcmips_fixmath PRIVATE -Wno-unused-parameter -Wno-missing-prototypes)

# Sync the public header into the sysroot so users can #include <fixmath/fix16.h>.
# The <fixmath/> prefix matches the include/fixmath/ source layout (equivalent to
# libcg's DEST_PREFIX pattern).
tcm_sync_headers_to_sysroot(tcmips_fixmath "${CMAKE_CURRENT_LIST_DIR}/include/")