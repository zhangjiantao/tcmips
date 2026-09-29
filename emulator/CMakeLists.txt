cmake_minimum_required(VERSION 3.16)
project(TCMIPSEmulator)

# ── Shared version & hardware config (single source of truth in cmake/) ──
# Loaded immediately after project() so every subsequent target setting can
# rely on TCMIPS_VERSION_STRING / TCM_MEMORY_SIZE etc. being available.
include(../cmake/version.cmake)
include(../cmake/config.cmake)
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Use the GNU dialect so GCC/Clang allow GNU extensions (computed goto in
# cpu_fast.c, __attribute__ in platform.h/memory.c/host.c).  MSVC does not
# honour CMAKE_C_STANDARD and always compiles as C; it uses the portable
# switch-dispatch path in cpu_fast.c and its own section/alignment support.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ── Emulator source files (organized by module) ───────────────────

add_executable(TCMIPSEmulator
    cpu_fast.c
    cpu_debug.c
    cpu_jit.c
    rsp_stub.c
    memory.c
    peripheral.c
    host.c
    platform.c
    main.c
    sljit/sljitLir.c
)

target_include_directories(TCMIPSEmulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../tcmips/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sljit
)

target_compile_options(TCMIPSEmulator PRIVATE -Wall -Wextra)

# ── Third-party: minifb (untouched — subdirectory manages itself) ─

add_subdirectory(minifb)
target_link_libraries(TCMIPSEmulator PRIVATE minifb)

# ── Windows: Winsock is required by the RSP stub on both MSVC and MinGW ─
# (rsp_stub.c uses socket()/bind()/listen()/accept(), which need ws2_32;
#  it also calls WSAStartup on entry.)

if (WIN32)
  target_link_libraries(TCMIPSEmulator PRIVATE ws2_32)
endif ()

# ── MinGW-specific size optimisation (MSVC has its own equivalent) ──────

if (MINGW)
  target_compile_options(TCMIPSEmulator PRIVATE
      -ffunction-sections
      -fdata-sections
      -fno-common
  )
  target_link_options(TCMIPSEmulator PRIVATE
      -static-libgcc
      -static-libstdc++
      -Wl,--gc-sections
  )
endif ()

# ── IDE project folder ───────────────────────────────────────────

set_target_properties(TCMIPSEmulator PROPERTIES
    FOLDER "Emulator"
)

# ── RSP stub debug logging ───────────────────────────────────────
# Logs every command packet / control byte received from the GDB debugger
# to stderr.  Equivalent to building with -DRSP_STUB_DEBUG.

set(RSP_STUB_DEBUG 0)
if (RSP_STUB_DEBUG)
  target_compile_definitions(TCMIPSEmulator PRIVATE -DRSP_STUB_DEBUG)
endif ()

set(JIT_ENABLE_FUNCTION_LEVEL_TRANSLATION 1)
if (JIT_ENABLE_FUNCTION_LEVEL_TRANSLATION)
  target_compile_definitions(TCMIPSEmulator PRIVATE -DJIT_ENABLE_FUNCTION_LEVEL_TRANSLATION)
endif ()

set(JIT_ENABLE_REG_BINDING 0)
if (JIT_ENABLE_REG_BINDING)
  target_compile_definitions(TCMIPSEmulator PRIVATE -DJIT_ENABLE_REG_BINDING)
endif ()
