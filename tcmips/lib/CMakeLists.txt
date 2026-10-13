add_subdirectory(clang-rt)
add_subdirectory(gcc-rt)
add_subdirectory(picolibc-1.8.11)
add_subdirectory(libcxxabi)
add_subdirectory(libcxx)

# -DNDEBUG -O3

# picolibc
target_compile_options(c PRIVATE -ffreestanding -nostdinc -fno-builtin -isystem ${TCM_CLANG_HEADERS} -Wno-macro-redefined)

# compiler_rt
target_compile_options(tcmips_clang_rt PRIVATE -D__ELF__)
target_link_libraries(tcmips_clang_rt PRIVATE c)

# libcxxabi
target_link_libraries(cxxabi_static PRIVATE c)

# libcxx
target_compile_options(cxx_static PRIVATE -fno-rtti)
target_link_libraries(cxx_static PRIVATE c)

# zlib
add_subdirectory(zlib)

# Compatibility Layers
add_subdirectory(libmemfs)
add_subdirectory(libsdl)
add_subdirectory(libsdl2)
add_subdirectory(libcg)
add_subdirectory(libstb)
add_subdirectory(libfixmath)
