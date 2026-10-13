# CMakeLists.txt for the TCMIPS libcg port (lightweight 2D graphics library).
#
# Original upstream repository:
#   https://github.com/xboot/libcg
# Exact upstream HEAD commit at the time of this import:
#   e1a0ae76f892089b2f89cc62e42a9b40754b1c5a
# Clone command used:
#   git clone https://github.com/xboot/libcg.git
# To reproduce this exact baseline:
#   cd libcg && git checkout e1a0ae76f892089b2f89cc62e42a9b40754b1c5a
#
# License: MIT (Copyright(c) Jianjun Jiang <8192542@qq.com>).
#   Full license text is vendored at LICENSE and must be preserved in any
#   redistribution (see the THIRD-PARTY NOTICES appendix of the root LICENSE).

add_library(tcmips_cg STATIC
    src/cg.c
    src/xft.c
)

target_include_directories(tcmips_cg PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# xft.h pulls in <stdlib.h>/<math.h>/<string.h>/... and both sources call
# malloc/free/sqrt etc., so libcg depends on picolibc (c) like the other libs.
target_link_libraries(tcmips_cg PRIVATE c)

# Upstream keeps headers flat in src/; DEST_PREFIX exposes them as <cg/cg.h>.
tcm_sync_headers_to_sysroot(tcmips_cg "${CMAKE_CURRENT_LIST_DIR}/src" DEST_PREFIX cg)

