# Automated boundary/stability self-test (printf-driven PASS/FAIL, exit code).
# Also hosts the bug/regression suites formerly built as standalone targets
# (maprgb / negclip / bounds / blitsrccclip / scalebug / upstream-fix checks).
add_executable(test_sdl2_autotest test_sdl2_autotest.c)
target_link_libraries(test_sdl2_autotest tcmips_core tcmips_sdl2)

if (TCM_BUILD_TEST_MISC)
  # Cartoon sprite / canvas showcase (blit, colorkey, stretch, renderer rotation).
  add_executable(test_sdl2_sprites test_sdl2_sprites.c)
  target_link_libraries(test_sdl2_sprites tcmips_core tcmips_sdl2)

  # 3D affine-transform showcase (rotating shaded cube).
  add_executable(test_sdl2_3d test_sdl2_3d.c)
  target_link_libraries(test_sdl2_3d tcmips_core tcmips_sdl2)

  # Embedded-memfs BMP asset animation (viewport stretch via RenderCopy NULL-dst,
  # 3x3 source tiles bouncing via RenderCopyF; 11 s).  The assets/ directory is
  # packed into the .tcm by the linker.
  add_executable(test_sdl2_bmp_anim test_sdl2_bmp_anim.c)
  target_link_libraries(test_sdl2_bmp_anim tcmips_core tcmips_sdl2)
  target_link_options(test_sdl2_bmp_anim PRIVATE
      -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

  add_executable(test_sdl2_demo test_sdl2_demo.c)
  target_link_libraries(test_sdl2_demo
      tcmips_core        # startup code + libc + dev (no SDL)
      tcmips_sdl2        # SDL2 compatibility layer (linked on demand)
  )

  # Visual check of SDL_RenderSetLogicalSize: a circle drawn in logical
  # coordinates keeps its on-screen size across changing logical resolutions.
  add_executable(test_sdl2_logicalsize test_sdl2_logicalsize.c)
  target_link_libraries(test_sdl2_logicalsize tcmips_core tcmips_sdl2)
endif ()
