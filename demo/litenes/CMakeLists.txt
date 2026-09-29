set(LITENES_SRCS
    src/main.c
    src/hal.c

    src/fce/common.c
    src/fce/cpu.c
    src/fce/cpu-addressing.c
    src/fce/fce.c
    src/fce/memory.c
    src/fce/mmc.c
    src/fce/ppu.c
    src/fce/psg.c
)

add_executable(LiteNES ${LITENES_SRCS})
target_link_options(LiteNES PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)
target_link_libraries(LiteNES PRIVATE tcmips_core)
target_include_directories(LiteNES PRIVATE include)
