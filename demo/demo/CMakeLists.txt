add_subdirectory(basictest)

add_executable(Demo
    main.c
    hash_md5.c
    hash_sha256.c
    raycast_softfp.c
    rotating_cube.c
    particle_simulation.c
    mandelbrotset.c
    screensaver.c
)

target_link_libraries(Demo tcmips_core basictest)
target_link_libraries(Demo ${CMAKE_CURRENT_SOURCE_DIR}/micropython/libMicroPython.bc)
