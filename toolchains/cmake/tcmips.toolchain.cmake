set(TCM_LINKER "${CMAKE_CURRENT_LIST_DIR}/../llvm/bin/llvm-link-tcmips${CMAKE_HOST_EXECUTABLE_SUFFIX}")
set(TCM_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../llvm/bin/clang${CMAKE_HOST_EXECUTABLE_SUFFIX}")
set(TCM_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/../llvm/bin/clang++${CMAKE_HOST_EXECUTABLE_SUFFIX}")
set(TCM_CLANG_HEADERS "${CMAKE_CURRENT_LIST_DIR}/../llvm/lib/clang/14.0.6/include")
set(TCM_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../sysroot")
set(TCM_TARGET_TRIPLE "mipsel-unknown-elf")
set(TCM_TARGET_ARCH "mipsel")

set(TCM_DEBUG_LINK_FLAG "")
if (DEFINED CACHE{CMAKE_BUILD_TYPE} AND NOT "$CACHE{CMAKE_BUILD_TYPE}" STREQUAL "")
  string(TOLOWER "$CACHE{CMAKE_BUILD_TYPE}" USER_BUILD_TYPE)
  if (USER_BUILD_TYPE STREQUAL "debug")
    set(TCM_DEBUG_LINK_FLAG "-tcm-debug=1")
  endif ()
endif ()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips) # picolibc
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SIZEOF_VOID_P 4)
set(CMAKE_C_SIZEOF_DATA_PTR 4)
set(CMAKE_CXX_SIZEOF_DATA_PTR 4)

set(CMAKE_SKIP_COMPATIBILITY_TESTS 1)
set(CMAKE_SIZEOF_CHAR 1)
set(CMAKE_SIZEOF_UNSIGNED_SHORT 2)
set(CMAKE_SIZEOF_SHORT 2)
set(CMAKE_SIZEOF_INT 4)
set(CMAKE_SIZEOF_UNSIGNED_LONG 4)
set(CMAKE_SIZEOF_UNSIGNED_INT 4)
set(CMAKE_SIZEOF_LONG 4)
set(CMAKE_SIZEOF_FLOAT 4)
set(CMAKE_SIZEOF_DOUBLE 8)
set(CMAKE_HAVE_LIMITS_H 1)
set(CMAKE_HAVE_UNISTD_H 1)
set(CMAKE_HAVE_PTHREAD_H 1)
set(CMAKE_HAVE_SYS_PRCTL_H 1)
set(CMAKE_WORDS_BIGENDIAN 0)
set(CMAKE_C_BYTE_ORDER "LITTLE_ENDIAN")
set(CMAKE_CXX_BYTE_ORDER "LITTLE_ENDIAN")
set(CMAKE_DL_LIBS)

set(CMAKE_FIND_ROOT_PATH ${TCM_SYSROOT})

set(CMAKE_CROSSCOMPILING TRUE)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

if (CMAKE_TOOLCHAIN_FILE)
endif ()

if (NOT CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
endif ()
if (NOT CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif ()
if (NOT CMAKE_FIND_ROOT_PATH_MODE_PACKAGE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif ()

set(CMAKE_C_COMPILER ${TCM_C_COMPILER})
set(CMAKE_CXX_COMPILER ${TCM_CXX_COMPILER})

set(CMAKE_LINKER ${TCM_LINKER})
set(CMAKE_C_COMPILER_LINKER ${TCM_LINKER})
set(CMAKE_CXX_COMPILER_LINKER ${TCM_LINKER})

set(CMAKE_AR ${TCM_LINKER})
set(CMAKE_C_COMPILER_AR ${TCM_LINKER})
set(CMAKE_CXX_COMPILER_AR ${TCM_LINKER})

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

set(TCM_OBJ_SUFFIX ".bc")
set(TCM_LIB_SUFFIX ".bc")
set(TCM_EXE_SUFFIX ".tcm")
set(CMAKE_C_OUTPUT_EXTENSION ${TCM_OBJ_SUFFIX})
set(CMAKE_CXX_OUTPUT_EXTENSION ${TCM_OBJ_SUFFIX})
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_PREFIX_C "lib")
set(CMAKE_STATIC_LIBRARY_PREFIX_CXX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ${TCM_LIB_SUFFIX})
set(CMAKE_STATIC_LIBRARY_SUFFIX_C ${TCM_LIB_SUFFIX})
set(CMAKE_STATIC_LIBRARY_SUFFIX_CXX ${TCM_LIB_SUFFIX})
set(CMAKE_EXECUTABLE_SUFFIX ${TCM_EXE_SUFFIX})
set(CMAKE_EXECUTABLE_SUFFIX_C ${TCM_EXE_SUFFIX})
set(CMAKE_EXECUTABLE_SUFFIX_CXX ${TCM_EXE_SUFFIX})
set(CMAKE_DL_LIBS "")

set(TCM_LINKER_VERBOSE "")
if (CMAKE_VERBOSE_MAKEFILE)
  set(TCM_LINKER_VERBOSE "-v")
endif ()

set(TCM_CREATE_LIBRARY "<CMAKE_AR> <OBJECTS> <LINK_FLAGS> -tcm-linkonly -o <TARGET> --color ${TCM_LINKER_VERBOSE}")
set(CMAKE_C_CREATE_STATIC_LIBRARY ${TCM_CREATE_LIBRARY})
set(CMAKE_CXX_CREATE_STATIC_LIBRARY ${TCM_CREATE_LIBRARY})

set(TCM_LINK_EXE "<CMAKE_LINKER> <OBJECTS> <LINK_LIBRARIES> <LINK_FLAGS> ${TCM_DEBUG_LINK_FLAG} -tcm-elf -o <TARGET> --color ${TCM_LINKER_VERBOSE}")
set(CMAKE_C_LINK_EXECUTABLE ${TCM_LINK_EXE})
set(CMAKE_CXX_LINK_EXECUTABLE ${TCM_LINK_EXE})

set(CMAKE_C_COMPILER_TARGET ${TCM_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${TCM_TARGET_TRIPLE})

set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

set(TCM_FLAGS "")
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
  set(TCM_HOST_WIN32 TRUE)
  set(TCM_FLAGS "-DTCM_HOST_WIN32")
elseif ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  set(TCM_HOST_APPLE TRUE)
  set(TCM_FLAGS "-DTCM_HOST_APPLE")
endif ()

set(TCM_FLAGS "${TCM_FLAGS} --target=mipsel-unknown-elf")
set(TCM_FLAGS "${TCM_FLAGS} -msoft-float -march=mips32r2 -mabi=32 -mno-abicalls")
set(TCM_FLAGS "${TCM_FLAGS} -mllvm -mno-check-zero-division")
set(TCM_FLAGS "${TCM_FLAGS} -emit-llvm")
#set(TCM_FLAGS "${TCM_FLAGS} -fvisibility=hidden")
set(TCM_FLAGS "${TCM_FLAGS} -D_FORTIFY_SOURCE=0")
set(TCM_FLAGS "${TCM_FLAGS} -D_DEFAULT_SOURCE=1 -D_GNU_SOURCE=1")
set(TCM_FLAGS "${TCM_FLAGS} -fno-use-cxa-atexit -fno-register-global-dtors-with-atexit")
set(TCM_FLAGS "${TCM_FLAGS} -funroll-loops")
#set(TCM_FLAGS "${TCM_FLAGS} -ffp-contract=off -fno-strict-aliasing -fno-tree-vectorize -fno-vectorize -fno-slp-vectorize -fno-unroll-loops")
set(TCM_FLAGS "${TCM_FLAGS} -Wall -Wextra -Wno-variadic-macros -Wno-strict-prototypes -Wno-unused-parameter -Wno-deprecated-declarations -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type")

set(CMAKE_C_FLAGS_INIT "${TCM_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${TCM_FLAGS} -fno-exceptions")

# When building in Debug mode, instruct clang to emit DWARF debug info into
# the LLVM IR bitcode (clang -g -> LLVM .bc carries !dbg metadata).  The
# linker then passes -tcm-debug=1 so that -tcm-elf carries the final DWARF.
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0 -gdwarf-4 -gno-column-info")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0 -gdwarf-4 -gno-column-info")

set(CMAKE_SYSROOT ${TCM_SYSROOT})
set(CMAKE_LIBRARY_ARCHITECTURE "${TCM_TARGET_TRIPLE}")


if (NOT "${CMAKE_PROJECT_NAME}" STREQUAL ""
    AND NOT "${CMAKE_PROJECT_NAME}" STREQUAL "tcmips_devkit"
    AND NOT "${CMAKE_PROJECT_NAME}" STREQUAL "CMAKE_TRY_COMPILE"
    AND NOT "${CMAKE_PROJECT_NAME}" MATCHES "Check")
  set(TCM_IS_USER_PROJECT TRUE)
else ()
  set(TCM_IS_USER_PROJECT FALSE)
endif ()

if (TCM_IS_USER_PROJECT)
  string(APPEND CMAKE_CXX_FLAGS_INIT " -fno-rtti -stdlib=libc++")

  set(TCM_LIB_DIR "${TCM_SYSROOT}/usr/lib/")
  if (NOT EXISTS "${TCM_LIB_DIR}")
    set(TCM_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../sysroot/usr/lib/")
  endif ()

  if (EXISTS "${TCM_LIB_DIR}")
    file(GLOB TCM_ALL_BC "${TCM_LIB_DIR}/*.bc")
  else ()
    message(WARNING "[TCM] Library directory not found: ${TCM_LIB_DIR}")
    set(TCM_ALL_BC "")
  endif ()

  set(TCM_EXCLUDED_LIBS
      "libtcmips_cg.bc"
      "libtcmips_sdl.bc"
      "libtcmips_sdl2.bc"
  )

  message(STATUS "[TCM] Optional targets:")
  if (NOT TARGET tcmips_cg AND EXISTS "${TCM_LIB_DIR}/libtcmips_cg.bc")
    add_library(tcmips_cg INTERFACE IMPORTED GLOBAL)
    set_property(TARGET tcmips_cg PROPERTY INTERFACE_LINK_LIBRARIES "${TCM_LIB_DIR}/libtcmips_cg.bc")
    message(STATUS "  -> tcmips_cg -> target_link_libraries(<tgt> PRIVATE tcmips_cg)")
  endif ()
  if (NOT TARGET tcmips_sdl AND EXISTS "${TCM_LIB_DIR}/libtcmips_sdl.bc")
    add_library(tcmips_sdl INTERFACE IMPORTED GLOBAL)
    set_property(TARGET tcmips_sdl PROPERTY INTERFACE_LINK_LIBRARIES "${TCM_LIB_DIR}/libtcmips_sdl.bc")
    message(STATUS "  -> tcmips_sdl -> target_link_libraries(<tgt> PRIVATE tcmips_sdl)")
  endif ()
  if (NOT TARGET tcmips_sdl2 AND EXISTS "${TCM_LIB_DIR}/libtcmips_sdl2.bc")
    add_library(tcmips_sdl2 INTERFACE IMPORTED GLOBAL)
    set_property(TARGET tcmips_sdl2 PROPERTY INTERFACE_LINK_LIBRARIES "${TCM_LIB_DIR}/libtcmips_sdl2.bc")
    message(STATUS "  -> tcmips_sdl2 -> target_link_libraries(<tgt> PRIVATE tcmips_sdl2)")
  endif ()

  set(TCM_FORCED_LIBS "")
  foreach (bc_file_path IN LISTS TCM_ALL_BC)
    get_filename_component(_bn "${bc_file_path}" NAME)
    if (NOT _bn IN_LIST TCM_EXCLUDED_LIBS)
      list(APPEND TCM_FORCED_LIBS "${bc_file_path}")
    endif ()
  endforeach ()

  if (TCM_FORCED_LIBS)
    set(TCM_FORCED_LINK_BC_STR "")
    foreach (bc_file_path IN LISTS TCM_FORCED_LIBS)
      set(TCM_FORCED_LINK_BC_STR "${TCM_FORCED_LINK_BC_STR} ${bc_file_path}")
    endforeach ()

    string(FIND "${CMAKE_EXE_LINKER_FLAGS_INIT}" "${TCM_FORCED_LINK_BC_STR}" _TCM_FIND_INDEX)
    if (_TCM_FIND_INDEX EQUAL -1)
      message(STATUS "[TCM] Automatically binding the following .bc files to all executables:")
      foreach (bc_file_path IN LISTS TCM_FORCED_LIBS)
        message(STATUS "  -> ${bc_file_path}")
      endforeach ()
      set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} ${TCM_FORCED_LINK_BC_STR}")
    endif ()
  else ()
    message(AUTHOR_WARNING "[TCM] No .bc files found in ${TCM_LIB_DIR}")
  endif ()
endif ()
