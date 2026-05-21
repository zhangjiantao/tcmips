add_subdirectory(clang-rt)
add_subdirectory(gcc-rt)
add_subdirectory(picolibc-1.8.11)
add_subdirectory(libcxxabi)
add_subdirectory(libcxx)

# picolibc
target_compile_options(c PRIVATE -DNDEBUG -O3 -ffreestanding -nostdinc -fno-builtin -isystem ${TCM_CLANG_HEADERS} -Wno-macro-redefined)
set_target_properties(c PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY})

# compiler_rt
target_compile_options(tcmips_clang_rt PRIVATE -D__ELF__ -DNDEBUG -O3)
add_dependencies(tcmips_clang_rt c)
set_target_properties(tcmips_clang_rt PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${TCM_ARCHIVE_OUTPUT_DIRECTORY})

# libcxxabi
target_compile_options(cxxabi_static PRIVATE -DNDEBUG -O3)
add_dependencies(cxxabi_static c)

# libcxx
target_compile_options(cxx_static PRIVATE -fno-rtti -DNDEBUG -O3)
add_dependencies(cxx_static c)
