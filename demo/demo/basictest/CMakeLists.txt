add_library(basictest
    test_arithmetic_builtin.c
    test_arithmetic_float.c
    test_arithmetic_int64.c
    test_aes.c
    test_sha256.c

    stdlib/test_malloc.c
    stdlib/test_malloc_stress.c
    stdlib/test_rand.c
    stdlib/test_regex.c
    stdlib/test_setjmp.c
    stdlib/test_strftime.c

    all.c
)

target_compile_options(basictest PRIVATE -DTESTALL)

#add_executable(testall main.c)
#target_link_libraries(testall PRIVATE tcmips_core basictest)
