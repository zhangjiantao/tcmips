# Automated boundary/stability self-test (printf-driven PASS/FAIL, exit code).
# Also hosts the bug/regression suites formerly built as standalone targets
# (blitclip / blitsrccclip / softstretch_bug and the upstream-fix checks).
add_executable(test_sdl1_autotest test_sdl1_autotest.c)
target_link_libraries(test_sdl1_autotest tcmips_core tcmips_sdl)

if (TCM_BUILD_TEST_MISC)
  add_executable(test_sdl1_hello test_sdl1_hello.c)
  target_link_libraries(test_sdl1_hello
      tcmips_core        # startup code + libc + dev + memfs (no SDL)
      tcmips_sdl         # SDL1 compatibility layer (self-contained, no SDL2)
  )

  add_executable(test_sdl1_demo_palette test_sdl1_demo_palette.c)
  target_link_libraries(test_sdl1_demo_palette tcmips_core tcmips_sdl)

  # Sprite blit / colorkey transparency / movement over a canvas.
  add_executable(test_sdl1_sprites test_sdl1_sprites.c)
  target_link_libraries(test_sdl1_sprites tcmips_core tcmips_sdl)

  # SDL_SoftStretch scaling and deformation (independent W/H sinusoids).
  add_executable(test_sdl1_stretch test_sdl1_stretch.c)
  target_link_libraries(test_sdl1_stretch tcmips_core tcmips_sdl)

  # Cross-format conversion (16/24/32 -> 32) and partial sub-rect blits.
  add_executable(test_sdl1_convert test_sdl1_convert.c)
  target_link_libraries(test_sdl1_convert tcmips_core tcmips_sdl)

  # Embedded-memfs BMP asset animation (stretch / 3x3 tiles / bounce; 11 s).
  # The assets/ directory is packed into the .tcm by the linker.
  add_executable(test_sdl1_bmp_anim test_sdl1_bmp_anim.c)
  target_link_libraries(test_sdl1_bmp_anim tcmips_core tcmips_sdl)
  target_link_options(test_sdl1_bmp_anim PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)
endif ()
