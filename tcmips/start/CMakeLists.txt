add_library(tcmips_start STATIC
    start.c
)

target_link_libraries(tcmips_start PRIVATE c tcmips_dev)
