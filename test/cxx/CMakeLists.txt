add_executable(test_cxx_autotest test_cxx_autotest.cpp)
target_link_libraries(test_cxx_autotest tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_autotest PRIVATE -fno-rtti -std=c++17)
