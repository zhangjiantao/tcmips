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

  set(AUTOTEST_TARGETS
      test_cg_autotest
      test_memfs_autotest
      test_memfs_autotest_cpp
      test_cxx_autotest
      test_sdl1_autotest
      test_sdl2_autotest
      test_picolibc_autotest
      test_zlib_autotest
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
