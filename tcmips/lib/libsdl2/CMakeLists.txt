# CMakeLists.txt for the TCMIPS SDL2 compatibility layer.
#
# Original upstream repository:
#   https://github.com/libsdl-org/SDL
# Branch used to obtain the public headers:
#   SDL2
# Exact upstream HEAD commit at the time of this import:
#   b90ac95029d801c5abc59472ba8e2200dff31e1e
#   Author: Frank Praznik <frank.praznik@gmail.com>
#   Date:   Wed Sep 2 12:15:21 2026 -0400
#   Summary: x11: Ignore BadWindow errors when handling a SelectionRequest
# Clone command used:
#   git clone --single-branch --branch SDL2 https://github.com/libsdl-org/SDL.git
# To reproduce this exact import:
#   cd SDL && git checkout b90ac95029d801c5abc59472ba8e2200dff31e1e
# We take only the public headers from upstream (include/*.h) and layer a new
# implementation on top of the TCMIPS device-sdk, NOT sdl2-compat.
#
# What this is / 设计思想
#   TCMIPS 是一个面向“图灵完备(Turing Complete)”游戏内虚拟 CPU 的
#   bare-metal MIPSel SDK，没有操作系统、没有 SDL2/GL/音频驱动。本项目
#   提供一个 SDL2 API 兼容层，让基于 SDL2 的开源小游戏只需少量修改，甚至
#   无需修改，就能用 TCMIPS 工具链编译并运行在 TCMIPS CPU 上。
#
#   它不同于 sdl2-compat（sdl2-compat 是把 SDL2 映射到 SDL3 后端，而
#   TCMIPS 并没有 SDL3 后端），也不同于完整 SDL2 移植；它只实现目标游戏
#   常用的子集（init/timer/event/video/surface/renderer/rwops/filesystem），
#   其余 API 均以统一的日志桩函数存在，避免链接失败并方便运行时排错。
#
# When to use / 使用场合
#   - 想用 SDL2 例程/小游戏迁移到 TCMIPS 场景验证
#   - 不想把 TCMIPS 平台细节（syscall、VRAM 地址、memfs）写进游戏代码
#   - 需要一套与 SDL2 一致的跨平台 API 作为 SDK 用户界面
#
# What it solves / 解决什么问题
#   - 解决“SDL2 头文件 + API 不存在于 TCMIPS SDK”的移植空白
#   - 把 TCMIPS 的 get_timestamp/get_timestamp_milli 封装成 SDL_GetTicks
#   - 把 TCMIPS 键盘事件封装成 SDL_PollEvent/SDL_KEYDOWN/SDL_KEYUP
#   - 把 TCMIPS 像素 VRAM 映射成 SDL_Window/SDL_Surface，并提供最小软件 renderer
#   - 文件 RWops 走 picolibc 文件操作（open/read/write/lseek/close），由 BSP
#     启动层的 libc syscall 胶水落到内存文件系统；本库不直接依赖 libmemfs。
#
# Integration / 集成方式
#   This file lives at tcmips/lib/libsdl2/CMakeLists.txt and is included via
#   add_subdirectory() from tcmips/lib/CMakeLists.txt.  It depends on targets
#   and variables from the parent build system:
#   - TCM_SYSROOT
#   - TCM_ARCHIVE_OUTPUT_DIRECTORY
#   - cmake/headers.cmake with tcm_sync_headers_to_sysroot
#   - tcmips_dev, c (picolibc)

add_library(tcmips_sdl2 STATIC
    src/tcmips_sdl2_state.c
    src/sdl2_init.c
    src/sdl2_timer.c
    src/sdl2_events.c
    src/sdl2_vram.c
    src/sdl2_video.c
    src/sdl2_surface.c
    src/sdl2_renderer.c
    src/sdl2_rect.c
    src/sdl2_rwops.c
    src/sdl2_filesystem.c
    src/sdl2_stdinc.c
    src/sdl2_error.c
    src/sdl2_log.c
    src/sdl2_hints.c
    src/sdl2_platform.c
    src/sdl2_stubs.c
)

# Private headers live under include/SDL2 so user code can #include <SDL.h>.
target_include_directories(tcmips_sdl2 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/SDL2>
)

# Internal helpers and TCMIPS device headers.
target_include_directories(tcmips_sdl2 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${TCM_SYSROOT}/usr/include>
)

target_link_libraries(tcmips_sdl2 PRIVATE
    tcmips_dev        # syscall / console / keyboard helpers
    c                 # picolibc (file verbs via BSP glue -> memfs)
)

# Mirror public headers into the sysroot so user programs see <SDL2/...>.
tcm_sync_headers_to_sysroot(tcmips_sdl2 "${CMAKE_CURRENT_SOURCE_DIR}/include/")

# Optional: make tcmips_core export tcmips_sdl2 for all SDK users.
# target_link_libraries(tcmips_core INTERFACE tcmips_sdl2)
