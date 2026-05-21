function(tcm_configure_file_to_sysroot SOURCE_NAME TARGET_NAME)
  set(SYSROOT_INCLUDE_DIR "${TCM_SYSROOT}/usr/include")
  configure_file(include/tcm_config.h.in ${SYSROOT_INCLUDE_DIR}/${TARGET_NAME})
endfunction()

function(tcm_sync_headers_to_sysroot TARGET_NAME SOURCE_DIR)
  set(SYSROOT_INCLUDE_DIR "${TCM_SYSROOT}/usr/include")

  file(GLOB_RECURSE ALL_HEADERS
      RELATIVE "${SOURCE_DIR}"
      "${SOURCE_DIR}/*.h"
      "${SOURCE_DIR}/*.hpp"
  )

  set(COPY_HEADER_OUTPUTS "")
  foreach(header_rel_path ${ALL_HEADERS})
    set(src_file "${SOURCE_DIR}/${header_rel_path}")
    set(dst_file "${SYSROOT_INCLUDE_DIR}/${header_rel_path}")

    add_custom_command(
        OUTPUT "${dst_file}"
        DEPENDS "${src_file}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src_file}" "${dst_file}"
        COMMENT "[${TARGET_NAME}] Syncing header: ${header_rel_path}"
    )
    list(APPEND COPY_HEADER_OUTPUTS "${dst_file}")
  endforeach()

  set(HEADER_TARGET_NAME "${TARGET_NAME}_sync_headers")
  add_custom_target(${HEADER_TARGET_NAME} DEPENDS ${COPY_HEADER_OUTPUTS})

  add_dependencies(${TARGET_NAME} ${HEADER_TARGET_NAME})
endfunction()