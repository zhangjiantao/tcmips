add_executable(PeanutGameBoy main.c gb_port.c)
target_link_libraries(PeanutGameBoy tcmips_core)
target_link_options(PeanutGameBoy PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)