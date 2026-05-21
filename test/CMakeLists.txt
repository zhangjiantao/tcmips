add_subdirectory(basic)
add_subdirectory(av)
add_subdirectory(cxx)


add_executable(test_keyboard test_keyboard.c)
target_link_libraries(test_keyboard tcmips_core)

add_executable(test_console stdio/test_stdio.c)
target_link_libraries(test_console tcmips_core)

add_executable(test_pi test_pi.c)
target_link_libraries(test_pi tcmips_core)

add_executable(test_pi2 test_pi2.c)
target_link_libraries(test_pi2 tcmips_core)

add_executable(test_tetris test_tetris.c)
target_link_libraries(test_tetris tcmips_core)

add_executable(test_raycast test_raycast.c)
target_link_libraries(test_raycast tcmips_core)

add_executable(test_stack_overflow test_stack_overflow.c)
target_link_libraries(test_stack_overflow tcmips_core)

add_executable(test_memxxx test_memxxx.c)
target_link_libraries(test_memxxx tcmips_core)

add_executable(test_divcost test_divcost.c)
target_link_libraries(test_divcost tcmips_core)

add_executable(test_jumptable test_jumptable.c)
target_link_libraries(test_jumptable tcmips_core)

