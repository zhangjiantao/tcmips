add_executable(test_basic
    test_arithmetic_builtin.c
    test_arithmetic_float.c
    test_arithmetic_int64.c
    test_aes.c
    test_sha256.c
    all.c
)

target_compile_options(test_basic PRIVATE -DTESTALL)

target_link_libraries(test_basic tcmips_core)