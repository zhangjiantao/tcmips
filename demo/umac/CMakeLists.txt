set(UMAC_SRCS
    src/disc.c
    src/main.c
    src/rom.c
    src/scc.c
    src/via.c

    src/tcmips_main.c

    external/Musashi/m68kcpu.c
    external/Musashi/m68kdasm.c
    external/Musashi/m68kops.c
    external/Musashi/softfloat/softfloat.c
)

add_executable(UMac ${UMAC_SRCS})

target_link_libraries(UMac tcmips_core)

target_include_directories(UMac PRIVATE
    include
    external/Musashi
    external/Musashi/softfloat
)

target_compile_definitions(UMac PRIVATE
    MUSASHI_CNF=\"../include/m68kconf.h\"

    UMAC_MEMSIZE=128

    DISP_WIDTH=512
    DISP_HEIGHT=342
)

