add_subdirectory(cxx)
add_subdirectory(stdio)

add_executable(emu emu.c)
target_link_libraries(emu tcmips_core)

add_executable(test_time test_time.c)
target_link_libraries(test_time tcmips_core)

add_executable(test_keyboard test_keyboard.c)
target_link_libraries(test_keyboard tcmips_core)

add_executable(test_console stdio/test_stdio.c)
target_link_libraries(test_console tcmips_core)

add_executable(test_pi test_pi.c)
target_link_libraries(test_pi tcmips_core)

add_executable(test_pi2 test_pi2.c)
target_link_libraries(test_pi2 tcmips_core)

add_executable(test_stack_overflow test_stack_overflow.c)
target_link_libraries(test_stack_overflow tcmips_core)

add_executable(test_memxxx test_memxxx.c)
target_link_libraries(test_memxxx tcmips_core)

add_executable(test_divcost test_divcost.c)
target_link_libraries(test_divcost tcmips_core)

add_executable(test_jumptable test_jumptable.c)
target_link_libraries(test_jumptable tcmips_core)

add_executable(test_segdisp test_segdisp.c)
target_link_libraries(test_segdisp tcmips_core)

add_executable(test_conwaygameoflife test_conwaygameoflife.c)
target_link_libraries(test_conwaygameoflife tcmips_core)

add_executable(test_palette test_palette.c)
target_link_libraries(test_palette tcmips_core)
