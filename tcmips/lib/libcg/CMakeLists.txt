add_library(tcmips_cg STATIC
    src/cg.c
    src/xft.c
)

target_include_directories(tcmips_cg PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

configure_file(src/xft.h ${TCM_SYSROOT}/usr/include/cg/xft.h)
configure_file(src/cg.h ${TCM_SYSROOT}/usr/include/cg/cg.h)

set_target_properties(tcmips_cg PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY})
set_target_properties(tcmips_cg PROPERTIES FOLDER "tcmips/cores")

