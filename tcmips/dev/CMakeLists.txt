add_library(tcmips_dev STATIC
    syscall.c
    seven_segment_display.c
    keyboard.c
    console.c
    console_unicode.c
    console_ascii.c
)

target_link_libraries(tcmips_dev PRIVATE c tcmips_zlib tcmips_memfs)

# Mirror the device headers (console/keyboard/syscall/...) into the sysroot.
tcm_sync_headers_to_sysroot(tcmips_dev "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# Note: On some CMake versions, you may need to set STATIC_LIBRARY_FLAGS for this link option to work.
set_target_properties(tcmips_dev
    PROPERTIES
    STATIC_LIBRARY_OPTIONS
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets/
)

# The -tcm-memfs asset directory is NOT tracked by the linker-option rule above,
# so a change to unifont.bin etc. would otherwise leave libtcmips_dev.bc stale.
# GLOB + CONFIGURE_DEPENDS makes CMake re-run configure (and therefore re-link
# the library) whenever the asset set changes.
file(GLOB_RECURSE TCM_DEV_ASSET_FILES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
