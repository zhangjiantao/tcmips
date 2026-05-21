set(TCM_ARCHIVE_OUTPUT_DIRECTORY "${TCM_SYSROOT}/usr/lib/")
set(TCM_RUNTIME_OUTPUT_DIRECTORY "${TCM_SYSROOT}/usr/bin/")

add_subdirectory(dev)
add_subdirectory(lib)
add_subdirectory(start)
add_subdirectory(loader)

include(cmake/version.cmake)
get_source_info("${CMAKE_CURRENT_SOURCE_DIR}/../" TCM_REVISION TCM_REPOSITORY TCM_AUTHOR)

include(cmake/headers.cmake)
tcm_configure_file_to_sysroot(include/tcm_config.h.in tcm_config.h)
tcm_sync_headers_to_sysroot(tcmips_dev "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(tcmips_core INTERFACE)

target_link_libraries(tcmips_core
    INTERFACE tcmips_start
    INTERFACE tcmips_clang_rt
    INTERFACE tcmips_dev
    INTERFACE c
)

add_library(tcmips_core_cxx INTERFACE)

target_link_libraries(tcmips_core_cxx
    INTERFACE tcmips_core
    INTERFACE cxx_static
    INTERFACE cxxabi_static
)

add_custom_target(all_target ALL
    COMMENT "Triggering full build for all bundled targets..."
)

add_dependencies(all_target
    tcm_bootloader
    tcmips_start
    tcmips_clang_rt
    tcmips_dev
    cxxabi_static
    cxx_static
    c
)