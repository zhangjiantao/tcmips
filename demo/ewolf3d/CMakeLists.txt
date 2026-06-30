set(EWOLF3D_SRCS
    ewolf3d/fmopl.c
    ewolf3d/id_ca.c
    ewolf3d/id_us.c
    ewolf3d/id_vh.c
    ewolf3d/misc.c
    ewolf3d/objs.c
    ewolf3d/sd_comm.c
    ewolf3d/sd_null.c
    ewolf3d/vi_bare.c
    ewolf3d/vi_comm.c
    ewolf3d/wl_act1.c
    ewolf3d/wl_act2.c
    ewolf3d/wl_act3.c
    ewolf3d/wl_agent.c
    ewolf3d/wl_debug.c
    ewolf3d/wl_draw.c
    ewolf3d/wl_game.c
    ewolf3d/wl_inter.c
    ewolf3d/wl_main.c
    ewolf3d/wl_menu.c
    ewolf3d/wl_play.c
    ewolf3d/wl_state.c
    ewolf3d/wl_text.c


    ewolf3d/sprites.c
    ewolf3d/walls.c
    ewolf3d/pagemap.c
    ewolf3d/tables.c
    ewolf3d/pal4bit.c
    ewolf3d/mapdata.c
)

add_executable(EWolf3D ${EWOLF3D_SRCS})
target_include_directories(EWolf3D PRIVATE ewolf3d ewolf3d/asserts)
target_compile_definitions(EWolf3D PRIVATE -DTCMIPS)
target_link_libraries(EWolf3D PRIVATE tcmips_core)