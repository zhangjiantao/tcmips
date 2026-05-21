add_library(tcmips_dev STATIC
    syscall.c
    seven_segment_display.c
    console.c
    keyboard.c
)

set_target_properties(tcmips_dev PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY})