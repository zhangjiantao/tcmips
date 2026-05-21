
add_executable(test_cxx_basic basic.cpp)
target_link_libraries(test_cxx_basic tcmips_core tcmips_core_cxx)
#target_link_options(test_cxx_basic PRIVATE -tcm-llvmir)
target_compile_options(test_cxx_basic PRIVATE -fno-rtti -std=c++17)

add_executable(test_cxx_fmt fmt.cpp)
target_link_libraries(test_cxx_fmt tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_fmt PRIVATE -fno-rtti -std=c++17)

add_executable(test_cxx_ctors ctors.cpp)
target_link_libraries(test_cxx_ctors tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_ctors PRIVATE -fno-rtti -std=c++17)

add_executable(test_cxx_stringview stringview.cpp)
target_link_libraries(test_cxx_stringview tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_stringview PRIVATE -fno-rtti -std=c++17)

add_executable(test_cxx_reduce reduce.cpp)
target_link_libraries(test_cxx_reduce tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_reduce PRIVATE -fno-rtti -std=c++17)

add_executable(test_cxx_lcm lcm.cpp)
target_link_libraries(test_cxx_lcm tcmips_core tcmips_core_cxx)
target_compile_options(test_cxx_lcm PRIVATE -fno-rtti -std=c++20)