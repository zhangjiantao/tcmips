# The memfs autotest drives the TCM_MEMFS_DEBUG-gated accounting APIs
# (memfs_debug_heap_bytes / memfs_debug_chunk_count) to prove the filesystem
# does not leak memory across create/write/unlink/truncate/init cycles.
#
# Whether those debug helpers exist is decided solely by the project-level
# TCM_MEMFS_DEBUG switch in the root CMakeLists.txt: when ON, libtcmips_memfs
# compiles the counters in and <tcm_config.h>'s #cmakedefine exposes
# TCM_MEMFS_DEBUG to consumers via <tcm_memfs.h>. When OFF, the lib omits them
# and test_memfs_autotest.c takes its SKIP branch (see section 11) so the test
# still builds, links and passes — it just doesn't exercise leak accounting.
# We therefore do NOT force-define -DTCM_MEMFS_DEBUG on either target here.

add_executable(test_memfs_autotest test_memfs_autotest.c)
target_link_libraries(test_memfs_autotest tcmips_core)
target_link_options(test_memfs_autotest PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

add_executable(test_memfs_autotest_cpp test_memfs_autotest.cpp)
target_link_libraries(test_memfs_autotest_cpp tcmips_core tcmips_core_cxx)
target_link_options(test_memfs_autotest_cpp PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)
target_compile_options(test_memfs_autotest_cpp PRIVATE -fno-rtti)
