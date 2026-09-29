add_subdirectory(demo)
add_subdirectory(doom)
add_subdirectory(gameboy)
add_subdirectory(litenes)
add_subdirectory(palnavy)
add_subdirectory(tiny386)
add_subdirectory(umac)

set(TCMIPS_DEMO_TARGETS
    Demo
    Doom
    Tiny386Win95
    UMac
    PeanutGameBoy
    LiteNES
    PalNavy
)

set_target_properties(${TCMIPS_DEMO_TARGETS}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/demo"
    FOLDER "tcmips/demo"
)

add_custom_target(TCMIPS_DEMO_ALL ALL COMMENT "Triggering full build for all demo targets...")
add_dependencies(TCMIPS_DEMO_ALL ${TCMIPS_DEMO_TARGETS})
set_target_properties(TCMIPS_DEMO_ALL PROPERTIES FOLDER "tcmips/demo")
