set(COREMARK_SRCS
    coremark/core_main.c
    coremark/core_matrix.c
    coremark/core_state.c
    coremark/core_util.c
    coremark/core_list_join.c

    coremark/simple/core_portme.c
)

add_executable(CoreMark ${COREMARK_SRCS})

target_compile_definitions(CoreMark PRIVATE
    ITERATIONS=200
    VALIDATION_RUN=1
#    CORE_DEBUG=1
    COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
    COMPILER_FLAGS="${CMAKE_C_FLAGS}"
    MEM_LOCATION="Turing-Complete-TCMIPS-RAM"
)

target_include_directories(CoreMark PRIVATE
    coremark
    coremark/simple
)

target_link_libraries(CoreMark tcmips_core)

