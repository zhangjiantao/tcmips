set(TCM_ARCHIVE_OUTPUT_DIRECTORY "${TCM_SYSROOT}/usr/lib/")
set(TCM_RUNTIME_OUTPUT_DIRECTORY "${TCM_SYSROOT}/usr/bin/")

include(../cmake/headers.cmake)
include(../cmake/version.cmake)
get_source_info("${CMAKE_CURRENT_SOURCE_DIR}/../" TCM_REVISION TCM_REPOSITORY TCM_AUTHOR)
tcm_configure_file_to_sysroot(include/tcm_config.h.in tcm_config.h)

add_subdirectory(dev)
add_subdirectory(lib)
add_subdirectory(bsp)

add_library(tcmips_core INTERFACE)

target_link_libraries(tcmips_core
    INTERFACE tcmips_bsp
    INTERFACE tcmips_clang_rt
    INTERFACE tcmips_dev
    INTERFACE tcmips_memfs
    INTERFACE tcmips_zlib
    INTERFACE c
)

add_library(tcmips_core_cxx INTERFACE)

target_link_libraries(tcmips_core_cxx
    INTERFACE tcmips_core
    INTERFACE cxx_static
    INTERFACE cxxabi_static
)

set(TCMIPS_CORE_TARGETS
    tcmips_bsp
    tcmips_clang_rt
    tcmips_dev
    tcmips_memfs
    tcmips_sdl
    tcmips_sdl2
    tcmips_cg
    tcmips_zlib
    tcmips_stb
    tcmips_fixmath
    cxxabi_static
    cxx_static
    c
)

set_target_properties(${TCMIPS_CORE_TARGETS}
    PROPERTIES
    FOLDER "tcmips/core"
    ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY}
)

add_custom_target(TCMIPS_CORE_ALL ALL COMMENT "Triggering full build for all bundled targets...")
add_dependencies(TCMIPS_CORE_ALL ${TCMIPS_CORE_TARGETS})
set_target_properties(TCMIPS_CORE_ALL PROPERTIES FOLDER "tcmips/core")
