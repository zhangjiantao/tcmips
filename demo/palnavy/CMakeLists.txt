set(PALNAVY_MINISDL 0)

set(PALNAVY_SRCS
    src/main.c
    src/battle/battle.c
    src/battle/fight.c
    src/device/input.c
    src/device/palette.c
    src/device/video.c
    src/game/game.c
    src/game/play.c
    src/game/script.c
    src/global/global.c
    src/global/palcfg.c
    src/global/palcommon.c
    src/global/res.c
    src/hal/FPS.c
    src/scene/map.c
    src/scene/scene.c
    src/text/font.c
    src/text/text.c
    src/ui/itemmenu.c
    src/ui/magicmenu.c
    src/ui/ui.c
    src/ui/uibattle.c
    src/ui/uigame.c

    src/stub.c
    src/stub_ndl.c
)

if (PALNAVY_MINISDL)
  list(APPEND PALNAVY_SRCS
      libminiSDL/src/audio.c
      libminiSDL/src/event.c
      libminiSDL/src/file.c
      libminiSDL/src/general.c
      libminiSDL/src/timer.c
      libminiSDL/src/video.c
  )
  set(PALNAVY_SDL)
  set(PALNAVY_SDL_INC libminiSDL/include)
else ()
  set(PALNAVY_SDL tcmips_sdl)
endif ()

list(APPEND PALNAVY_SRCS
    src/misc/aviplay.c
    src/misc/ending.c
    src/misc/rngplay.c
    src/misc/util.c
    src/misc/yj1.c
)

add_executable(PalNavy ${PALNAVY_SRCS})
target_include_directories(PalNavy PRIVATE
    include
    src/text
    ${PALNAVY_SDL_INC}
)
target_compile_definitions(PalNavy PRIVATE
    -DPAL_NO_LAUNCH_UI=1
    -D__NAVY__=1
    -DPALNAVY_MINISDL=${PALNAVY_MINISDL}
)
target_link_libraries(PalNavy PRIVATE  tcmips_core ${PALNAVY_SDL})
target_link_options(PalNavy PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

#add_executable(PalNavyPLT ${PALNAVY_SRCS})
#target_include_directories(PalNavyPLT PRIVATE
#    include
#    src/text
#    ${PALNAVY_SDL_INC}
#)
#target_compile_definitions(PalNavyPLT PRIVATE
#    -DPAL_NO_LAUNCH_UI=1
#    -D__NAVY__=1
#    -DPALNAVY_MINISDL=${PALNAVY_MINISDL}
#    -DQ256_HARD_PALETTE
#)
#target_link_libraries(PalNavyPLT PRIVATE tcmips_core ${PALNAVY_SDL})
#target_link_options(PalNavyPLT PRIVATE -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

#set_source_files_properties(src/scene/scene.c PROPERTIES COMPILE_FLAGS "-O0")

#get_target_property(MY_SOURCES PalNavyPLT SOURCES)
#set(COUNTER 0)
#set(SPLIT_LIMIT 13)
#
#foreach(SOURCE_FILE ${MY_SOURCES})
#  if(SOURCE_FILE MATCHES "\\.c$")
#    if(COUNTER LESS SPLIT_LIMIT)
#      set_source_files_properties(${SOURCE_FILE} PROPERTIES COMPILE_FLAGS "-O0")
#      message(STATUS "[O0-Debug] Forcing -O0 on: ${SOURCE_FILE}")
#    else()
#      message(STATUS "[O1-Debug] Keeping -O1 on: ${SOURCE_FILE}")
#    endif()
#    math(EXPR COUNTER "${COUNTER} + 1")
#  endif()
#endforeach()

