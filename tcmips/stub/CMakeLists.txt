add_library(tcmips_stub STATIC
    stub.c
)

target_link_libraries(tcmips_stub PRIVATE c)
