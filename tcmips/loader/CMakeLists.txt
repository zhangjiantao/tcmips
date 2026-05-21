add_executable(tcm_bootloader bootloader.c)

target_link_libraries(tcm_bootloader PRIVATE
    tcmips_dev
)

target_link_options(tcm_bootloader PRIVATE
    -tcm-imgbase=0x100
    -tcm-imgsize=0x4000
)

target_compile_options(tcm_bootloader PRIVATE
    -fno-delete-null-pointer-checks
)

set_target_properties(tcm_bootloader PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TCM_RUNTIME_OUTPUT_DIRECTORY})