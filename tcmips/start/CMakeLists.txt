add_library(tcmips_start STATIC
    stub.c
    start.c
)

target_link_libraries(tcmips_start PRIVATE c)
set_target_properties(tcmips_start PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY})