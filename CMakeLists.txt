cmake_minimum_required(VERSION 3.31)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchains/cmake/tcmips.toolchain.cmake"
    CACHE FILEPATH "Path to toolchain file" FORCE)

project(tcmips_devkit)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ============================================================================
# Configuration (single sources of truth in cmake/)
# ============================================================================

# Version is shared across the guest cross-build and the host emulator build.
include(cmake/version.cmake)

# Hardware configuration (memory sizes, address map), shared with the emulator
# and downstream projects.
include(cmake/config.cmake)

if (NOT DEFINED TCM_CONSOLE_ASCII_FONT)
  set(TCM_CONSOLE_ASCII_FONT OFF)
endif ()
if (NOT DEFINED TCM_CONSOLE_UNICODE_FONT AND NOT TCM_CONSOLE_ASCII_FONT)
  set(TCM_CONSOLE_UNICODE_FONT ON)
endif ()

# Debug instrumentation for SDK compat libs (libmemfs/libsdl/libsdl2). Exposed via
# tcm_config.h, so no -D needed. TCM_SDL_DEBUG = SDL leak tracking; TCM_MEMFS_DEBUG = heap accounting.
if (NOT DEFINED TCM_SDL_DEBUG)
  set(TCM_SDL_DEBUG ON)
endif ()

if (NOT DEFINED TCM_MEMFS_DEBUG)
  set(TCM_MEMFS_DEBUG ON)
endif ()

if (NOT DEFINED TCM_BUILD_TEST_AUTOTEST)
  set(TCM_BUILD_TEST_AUTOTEST ON)
endif ()

if (NOT DEFINED TCM_BUILD_TEST_MISC)
  set(TCM_BUILD_TEST_MISC ON)
endif ()

# ============================================================================
# Tooling
# ============================================================================

# Needed by test/zlib (generate_ref_data.py) and by PACK_RELEASE
# (tools/release/pack_release.py); located before the directories that use it.
find_program(TCM_PYTHON3 NAMES python3 python)

# ============================================================================
# Subdirectories
# ============================================================================

add_subdirectory(tcmips)
add_subdirectory(demo)
add_subdirectory(test)

# ============================================================================
# Host emulator + release packaging (not attached to ALL)
# ============================================================================

# TCMIPSEmulator is an independent host project (emulator/CMakeLists.txt). Do not
# add_subdirectory it here — this project is cross-compiled and would build host
# code with the mipsel toolchain. Instead invoke its pre-configured build dir.
set(TCM_EMULATOR_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/emulator/cmake-build-release")
add_custom_target(EMULATOR
    COMMAND "${CMAKE_COMMAND}" --build "${TCM_EMULATOR_BUILD_DIR}" --target TCMIPSEmulator
    COMMENT "Build host-side TCMIPSEmulator (independent host project)"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(EMULATOR PROPERTIES FOLDER "tcmips/emulator")
if (NOT EXISTS "${TCM_EMULATOR_BUILD_DIR}/CMakeCache.txt")
  message(WARNING "TCMIPSEmulator build dir not configured (no CMakeCache.txt). Configure it first: cmake -G Ninja -B emulator/cmake-build-release -DCMAKE_BUILD_TYPE=Release")
endif ()

# Standalone packing target: NOT attached to ALL, triggered only by an
# explicit "cmake --build --target PACK_RELEASE". Collects + archives the
# release artifacts built by TCMIPS_CORE_ALL/TCMIPS_DEMO_ALL/TCMIPSEmulator.
add_custom_target(PACK_RELEASE
    COMMAND "${TCM_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/release/pack_release.py"
    --version "${TCMIPS_VERSION_STRING}"
    --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
    --out "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    DEPENDS TCMIPS_CORE_ALL TCMIPS_DEMO_ALL EMULATOR
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Pack TCMIPS release artifacts"
)
set_target_properties(PACK_RELEASE PROPERTIES FOLDER "tcmips/release")
