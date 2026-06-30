add_executable(GameBoyMarioLand main.c gb_port.c)
target_link_libraries(GameBoyMarioLand tcmips_core)
target_compile_options(GameBoyMarioLand PRIVATE -DMARIO)

add_executable(GameBoyDrMario main.c gb_port.c)
target_link_libraries(GameBoyDrMario tcmips_core)
target_compile_options(GameBoyDrMario PRIVATE -DDRMARIO)

add_executable(GameBoyKirby main.c gb_port.c)
target_link_libraries(GameBoyKirby tcmips_core)
target_compile_options(GameBoyKirby PRIVATE -DKIRBY)

add_executable(GameBoyZelda main.c gb_port.c)
target_link_libraries(GameBoyZelda tcmips_core)
target_compile_options(GameBoyZelda PRIVATE -DZELDA)
