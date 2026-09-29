# libtcmips_memfs - in-memory filesystem runtime (see tcm_memfs.h).
add_library(tcmips_memfs STATIC
    memfs.c
)

target_link_libraries(tcmips_memfs PRIVATE c)
