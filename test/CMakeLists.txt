if (TCM_BUILD_TEST_MISC)
  add_subdirectory(misc)
endif ()

if (TCM_BUILD_TEST_AUTOTEST)
  add_subdirectory(cg)
  add_subdirectory(cxx)
  add_subdirectory(picolibc)
  add_subdirectory(memfs)
  add_subdirectory(sdl2)
  add_subdirectory(sdl1)
  add_subdirectory(zlib)
  add_subdirectory(stb)
  add_subdirectory(dev)
  add_subdirectory(stdin)
  add_subdirectory(fixmath)

  set(AUTOTEST_TARGETS
      test_cg_autotest
      test_memfs_autotest
      test_memfs_autotest_cpp
      test_bsp_fileio_autotest
      test_cxx_autotest
      test_sdl1_autotest
      test_sdl2_autotest
      test_picolibc_autotest
      test_zlib_autotest
      test_stb_image_autotest
      test_stb_image_resize2_autotest
      test_stb_truetype_autotest
      test_stb_ds_autotest
      test_stb_sprintf_autotest
      test_stb_rect_pack_autotest
      test_dev_font_autotest
      # NOTE: test_stdin_autotest verifies the semihost INPUT path; it needs a
      # runtime `-stdin=<file>` to exercise anything. Without it the guest
      # prints "[SKIP] stdin-wired skipped" and exits 0 (see test/stdin/).
      test_stdin_autotest
      test_fixmath_autotest
  )

  set_target_properties(${AUTOTEST_TARGETS}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/autotest"
      FOLDER "tcmips/autotest"
  )

  add_custom_target(AUTOTEST_ALL ALL COMMENT "Triggering full build for all autotest targets...")
  add_dependencies(AUTOTEST_ALL ${AUTOTEST_TARGETS})
  set_target_properties(AUTOTEST_ALL PROPERTIES FOLDER "tcmips/autotest")
endif ()
