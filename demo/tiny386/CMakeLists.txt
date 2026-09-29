set(TINY386_CORE_SRCS
    i386.c
    fpu.c

    pc.c
    i8259.c      # 中断
    i8254.c      # 定时器
    i8257.c      # DMA
    i8042.c      # 键盘鼠标
    ide.c        # 磁盘
    pci.c
    misc.c
    vga.c
    main.c
)

add_executable(Tiny386Win95 ${TINY386_CORE_SRCS})
target_link_libraries(Tiny386Win95 tcmips_core)
target_include_directories(Tiny386Win95 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(Tiny386Win95 PRIVATE $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/tcmips.h>)
target_link_options(Tiny386Win95 PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)
target_compile_definitions(Tiny386Win95 PRIVATE
    I386_ENABLE_FPU=1
    #    I386_ENABLE_MMX=0
    #    I386_ENABLE_SSE=0
    #    I386_ENABLE_SSE2=0
    #    I386_ENABLE_SSE3=0
    #    I386_ENABLE_SSSE3=0
    TCM_TINY386_WIN95=1

    TCM_TINY386_KBD=1
)

