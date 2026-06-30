add_subdirectory(demo)
add_subdirectory(doom)
add_subdirectory(gameboy)
add_subdirectory(litenes)
add_subdirectory(palnavy)
add_subdirectory(campaign)
add_subdirectory(ewolf3d)
add_subdirectory(tiny386)
add_subdirectory(umac)
add_subdirectory(coremark)

add_custom_target(tcmips_demos_all ALL
    COMMENT "Triggering full build for all demo targets..."
)

add_dependencies(tcmips_demos_all
    Demo
    Doom
    DoomQ256
    DoomOBD
    DoomPLT
    EWolf3D
    Tiny386Win95
    UMac
    CoreMark
    GameBoyZelda
    GameBoyMarioLand
    GameBoyKirby
    GameBoyDrMario
    LiteNESAdvIsland
    LiteNESCircus
    LiteNESSuperMarioBro
    PalNavy
    PalNavyPLT
)

set_target_properties(
    tcmips_demos_all
    Demo
    Doom
    DoomQ256
    DoomOBD
    DoomPLT
    EWolf3D
    Tiny386Win95
    UMac
    CoreMark
    GameBoyZelda
    GameBoyMarioLand
    GameBoyKirby
    GameBoyDrMario
    LiteNESAdvIsland
    LiteNESCircus
    LiteNESSuperMarioBro
    PalNavy
    PalNavyPLT
    PROPERTIES FOLDER "tcmips/demos"
)
