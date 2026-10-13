# libtcmips_memfs - in-memory filesystem runtime (see memfs/memfs.h).
add_library(tcmips_memfs STATIC
    memfs.c
)

# Public headers live under include/memfs so user code can
# #include <memfs/memfs.h>. memfs_abi.h is also mirrored here for the linker.
target_include_directories(tcmips_memfs PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(tcmips_memfs PRIVATE c)

# Mirror public headers into the sysroot so user programs see <memfs/...>.
tcm_sync_headers_to_sysroot(tcmips_memfs "${CMAKE_CURRENT_SOURCE_DIR}/include/")
