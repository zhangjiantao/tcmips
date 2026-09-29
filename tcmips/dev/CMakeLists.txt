add_library(tcmips_dev STATIC
    syscall.c
    seven_segment_display.c
    keyboard.c
    console.c
    console_unicode.c
    console_ascii.c
)

target_link_libraries(tcmips_dev PRIVATE c tcmips_zlib)

# Note: On some CMake versions, you may need to set STATIC_LIBRARY_FLAGS for this link option to work.
set_target_properties(tcmips_dev
    PROPERTIES
    STATIC_LIBRARY_OPTIONS
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets/
)
