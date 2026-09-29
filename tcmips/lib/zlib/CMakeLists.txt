# CMakeLists.txt for the TCMIPS zlib port.
#
# Original upstream repository:
#   https://github.com/madler/zlib
# Exact upstream release used as the public API / ABI baseline:
#   zlib 1.3.1
#   Date:   2024-05-22
#   Commit: c2345725c5810f2b3d2f7d5b1da463f0c52db86d
#   Author: Mark Adler <madler@alumni.stanford.edu>
# Clone command used:
#   git clone --single-branch --branch v1.3.1 \
#     https://github.com/madler/zlib.git
# To reproduce this exact baseline:
#   cd zlib && git checkout c2345725c5810f2b3d2f7d5b1da463f0c52db86d

# Functionality: full zlib 1.3.1 static library (no demo), including gzip stream support.
#   - inflate/deflate (core compression/decompression)
#   - crc32/adler32 (checksums)
#   - compress/uncompress/compressBound (whole-memory compression)
#   - zlibVersion/zlibCompileFlags (version and compile flags)
#   - gz* (gzip stream I/O: gzlib.c, gzread.c, gzwrite.c, gzclose.c)
#
# Notes:
#   - No trimming: all API functions are kept (zlib is designed for portability).
#   - HAVE_UNISTD_H: enables unistd.h include in zconf.h (provides lseek/read/write/
#     open/SEEK_*/off_t declarations used by gz* sources via picolibc).
#   - _LARGEFILE64_SOURCE intentionally NOT defined: picolibc sys/unistd.h only
#     declares lseek (not lseek64); TCMIPS is 32-bit, no LFS needed.
#   - gzip sources depend on picolibc stdio.h (SEEK_SET/CUR/END, off_t) and
#     sys/unistd.h (lseek, read, write, open, close).
#   - Requires picolibc malloc/free (ENABLE_MALLOC).
#
# Integration: included via add_subdirectory() from tcmips/lib/CMakeLists.txt.
#   Dependencies: c (picolibc: stdio/stdlib/string/unistd)
#   Public header: zlib.h (synced to sysroot via tcm_sync_headers_to_sysroot).
#
# License: zlib License (C) 1995-2022 Jean-loup Gailly and Mark Adler.
#   Full license text is vendored at src/LICENSE and must be preserved in any
#   redistribution (see the THIRD-PARTY NOTICES appendix of the root LICENSE).

add_library(tcmips_zlib STATIC
    src/adler32.c
    src/compress.c
    src/crc32.c
    src/deflate.c
    src/gzclose.c
    src/gzlib.c
    src/gzread.c
    src/gzwrite.c
    src/infback.c
    src/inffast.c
    src/inflate.c
    src/inftrees.c
    src/trees.c
    src/uncompr.c
    src/zutil.c
)

target_include_directories(tcmips_zlib
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>        # zlib.h, zconf.h (root)
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>    # internal .h
)

target_link_libraries(tcmips_zlib
    PUBLIC c  # picolibc: stdio, stdlib, string, unistd, malloc/free
)

# ZLIB_INTERNAL: defined internally by zlib sources (zutil.h:19/empty, gzguts.h:17/empty).
#   Do NOT define here — would cause redefinition warning with source #define ZLIB_INTERNAL.
# HAVE_UNISTD_H: ensures unistd.h declarations available for gz* sources.
target_compile_definitions(tcmips_zlib
    PRIVATE
    HAVE_UNISTD_H
)

target_compile_options(tcmips_zlib PRIVATE -Wno-missing-prototypes -Wno-c99-extensions)

# Sync public headers (zlib.h and zconf.h at root) into the sysroot so user
# programs can #include <zlib.h>. Internal headers under src/ are NOT synced.
tcm_sync_headers_to_sysroot(tcmips_zlib "${CMAKE_CURRENT_LIST_DIR}")
