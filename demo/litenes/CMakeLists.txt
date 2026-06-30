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

add_executable(LiteNESSuperMarioBro ${LITENES_SRCS})
target_link_libraries(LiteNESSuperMarioBro PRIVATE tcmips_core)
target_include_directories(LiteNESSuperMarioBro PRIVATE include)
target_compile_definitions(LiteNESSuperMarioBro PRIVATE -DROM_SUPERMARIO)

add_executable(LiteNESAdvIsland ${LITENES_SRCS})
target_link_libraries(LiteNESAdvIsland PRIVATE tcmips_core)
target_include_directories(LiteNESAdvIsland PRIVATE include)
target_compile_definitions(LiteNESAdvIsland PRIVATE -DROM_ADVISLAND)

add_executable(LiteNESCircus ${LITENES_SRCS})
target_link_libraries(LiteNESCircus PRIVATE tcmips_core)
target_include_directories(LiteNESCircus PRIVATE include)
target_compile_definitions(LiteNESCircus PRIVATE -DROM_CIRCUS)
