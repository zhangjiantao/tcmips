# TCMIPS board-support package (BSP).
#
# Brings together the startup glue and the POSIX syscall/stub layer that
# bridges picolibc's assumed POSIX interfaces (open/read/write/_exit/...) onto
# the platform: the in-memory filesystem (libmemfs) for fds >= MEMFS_BASE_FD,
# the console for stdin/stdout/stderr, and the emulator's semihost bridge.
#
# The two source files form one logical unit — consumers link tcmips_core
# (which includes this target) and never reference tcmips_bsp directly, so a
# single STATIC library is the right granularity.
#
#   start.c  — boot entry (TCM_SYMBOL_START), heap bounds, memfs_init().
#   posix.c  — POSIX API implementation (open/read/write/lseek/close/stat/...).

add_library(tcmips_bsp STATIC
    start.c
    posix.c
)

target_link_libraries(tcmips_bsp PRIVATE c tcmips_dev tcmips_memfs)

# Pack the boot-logo pixel asset (assets/tcmips/logo/logo.bin) into the image
# so start.c can memfs_map() it at boot. Uses the same -tcm-memfs mechanism as
# tcmips_dev's bundled font asset.
set_target_properties(tcmips_bsp
    PROPERTIES
    STATIC_LIBRARY_OPTIONS
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets/
)

# Track the asset directory so a change to logo.bin re-runs configure and
# re-links libtcmips_bsp.bc (the -tcm-memfs option alone is untracked).
file(GLOB_RECURSE TCM_BSP_ASSET_FILES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")

# Mirror the BSP-facing header (tcm_bsp.h: MEMFS_BASE_FD etc.) into the sysroot.
tcm_sync_headers_to_sysroot(tcmips_bsp "${CMAKE_CURRENT_SOURCE_DIR}/include/")