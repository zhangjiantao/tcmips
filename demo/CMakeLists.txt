add_executable(demo
    main.c
    hash_md5.c
    hash_sha256.c
    music_player.c
    image_viewer.c
    video_player.c
    tetris_game.c
    raycast_softfp.c
)

target_link_libraries(demo tcmips_core)
target_link_libraries(demo ${CMAKE_CURRENT_SOURCE_DIR}/micropython/libMicroPython.bc)