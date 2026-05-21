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

if (TCM_HOST_APPLE)
  target_link_libraries(demo /Users/zhangjiantao/micropython-1.28.0/ports/tcmips/build/libMicroPython.bc)
elseif (TCM_HOST_WIN32)
  target_link_libraries(demo D:/code/tcmips-micropython-1.28.0/ports/tcmips/build/libMicroPython.bc)
endif ()