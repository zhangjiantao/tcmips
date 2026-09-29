# CMakeLists.txt for the TCMIPS zlib autotest.
#
# Original upstream repository for the tested library:
#   https://github.com/madler/zlib
# Exact upstream release used as the public API / ABI baseline:
#   zlib 1.3.1
#   Date:   2024-05-22
#   Commit: c2345725c5810f2b3d2f7d5b1da463f0c52db86d
#   Author: Mark Adler <madler@alumni.stanford.edu>
# Clone command used:
#   git clone --single-branch --branch v1.3.1 \
#     https://github.com/madler/zlib.git
# To reproduce this exact baseline:
#   cd zlib && git checkout c2345725c5810f2b3d2f7d5b1da463f0c52db86d

add_executable(test_zlib_autotest test_zlib_autotest.c)
target_link_libraries(test_zlib_autotest tcmips_zlib tcmips_core)

# Embed test/zlib/assets/ into the TCM image via memfs.
# Files available in-guest at:
#   zlib_data/level_{0,3,6,9}_{empty,text,repeat}.zlib -- Python-compressed
#     streams for inflate roundtrip tests (cross-language robustness).
#   raw/{empty,text,repeat}.raw -- original data for gz* family tests.
target_link_options(test_zlib_autotest PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

# Auto-generate test data via generate_ref_data.py before building.
# Data under test/zlib/assets/ is runtime output (gitignored), so this
# custom target regenerates it every build to keep it up to date.
if (TCM_PYTHON3)
  add_custom_target(_zlib_gen_data ALL
    COMMAND "${TCM_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_ref_data.py"
    COMMENT "Generating zlib autotest reference data..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  add_dependencies(test_zlib_autotest _zlib_gen_data)
endif ()