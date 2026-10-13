add_executable(test_time test_time.c)
target_link_libraries(test_time tcmips_core)

add_executable(test_keyboard test_keyboard.c)
target_link_libraries(test_keyboard tcmips_core)

add_executable(test_pi test_pi.c)
target_link_libraries(test_pi tcmips_core)

add_executable(test_pi2 test_pi2.c)
target_link_libraries(test_pi2 tcmips_core)

add_executable(test_stack_overflow test_stack_overflow.c)
target_link_libraries(test_stack_overflow tcmips_core)

add_executable(test_memxxx test_memxxx.c)
target_link_libraries(test_memxxx tcmips_core)

add_executable(test_jumptable test_jumptable.c)
target_link_libraries(test_jumptable tcmips_core)

add_executable(test_segdisp test_segdisp.c)
target_link_libraries(test_segdisp tcmips_core)

add_executable(test_conwaygameoflife test_conwaygameoflife.c)
target_link_libraries(test_conwaygameoflife tcmips_core)

add_executable(test_palette test_palette.c)
target_link_libraries(test_palette tcmips_core)

add_executable(test_float_pi test_float_pi.c)
target_link_libraries(test_float_pi tcmips_core)

add_executable(test_unicode test_unicode.c)
target_link_libraries(test_unicode tcmips_core)

# --- raycast 基准：完整 320×240 球求交，float vs libfixmath ---------------
add_executable(test_raycast_float test_raycast.c)
target_link_libraries(test_raycast_float tcmips_core)

add_executable(test_raycast_fixed test_raycast.c)
target_link_libraries(test_raycast_fixed tcmips_core tcmips_fixmath)
target_compile_definitions(test_raycast_fixed PRIVATE BENCH_FIXED=1)

# --- stb_image GIF playback demos (NOT part of the AUTOTEST suite) -----------
# Interactive demos: load test.gif and loop it automatically (no key press
# needed), press 'q' to exit. One demo per SDL compatibility layer:
#
#   test_gif_demo_sdl1  — SDL1 (tcmips_sdl): SDL_SetVideoMode + SDL_SoftStretch
#   test_gif_demo_sdl2  — SDL2 (tcmips_sdl2): SDL_CreateRenderer + SDL_UpdateTexture
#
# Both use tcmips_stb (stb_image) for the GIF loader. The GIF asset is read at
# runtime via memfs from assets/; `*.gif` is gitignored (see .gitignore), so
# the developer supplies the actual GIF asset locally.
add_executable(test_gif_demo_sdl1 test_gif_demo_sdl1.c)
target_link_libraries(test_gif_demo_sdl1 tcmips_core tcmips_sdl tcmips_stb)
target_link_options(test_gif_demo_sdl1 PRIVATE
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

add_executable(test_gif_demo_sdl2 test_gif_demo_sdl2.c)
target_link_libraries(test_gif_demo_sdl2 tcmips_core tcmips_sdl2 tcmips_stb)
target_link_options(test_gif_demo_sdl2 PRIVATE
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)
