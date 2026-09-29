# CMakeLists.txt for the TCMIPS SDL compatibility layer.
#
# Original upstream reference:
#   https://github.com/libsdl-org/SDL-1.2
# Exact upstream release used as the public API / ABI baseline:
#   release-1.2.15
#   Commit: 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
#   Author: Linden <ich@...>
#   Date:   Sun Jan 29 03:33:50 2012 -0800
# Clone command used:
#   git clone --single-branch --branch main https://github.com/libsdl-org/SDL-1.2.git
# To reproduce this exact baseline:
#   cd SDL-1.2 && git checkout release-1.2.15
#
# Implementation also consulted (for API semantics only, NOT forked):
#   https://github.com/libsdl-org/sdl12-compat
#   release-1.2.68
#   Commit: 35ef746cf1a4fde8c442dc92fe9b18c3f90d7855
#
# What this is / 设计思想
#   本库（libsdl）是 TCMIPS 专用的 SDL 兼容层，但它【不依赖 SDL2】：
#   它直接基于 TCMIPS 平台原生核心组件（tcmips_dev / picolibc）实现。
#   视频层直接驱动硬件像素台（tcm_pixel_console_init 返回的物理
#   VRAM 帧缓冲），SDL1 内部持有的逻辑帧缓冲在 SDL_Flip / SDL_UpdateRect
#   时直接提交到物理 VRAM，避免了"SDL1 → SDL2 window surface"这层多余的
#   缓冲交换（相比 SDL2 兼容层省去一次整屏拷贝，热路径退化为 1:1 memcpy）。
#
#   核心取舍：
#     - 保留 SDL 的 surface-oriented 视频模型（SDL_SetVideoMode / SDL_Flip）。
#     - 键盘事件直接读取 TCMIPS 键盘 syscall（tcm_syscall_read_keyboard）。
#     - 计时直接使用 TCMIPS 时间戳 syscall（tcm_syscall_get_timestamp_*）。
#     - 文件 RWops 走 picolibc 文件操作（open/read/write/lseek/close），
#       由 BSP 启动层的 libc syscall 胶水落到内存文件系统；
#       本库不直接依赖 libmemfs 的内部约定。
#     - 不实现 OpenGL、joystick、haptic、CD-ROM；这些子系统返回空或错误。
#     - 音频实现为 dummy audio：允许注册回调但不产生真实声音输出。
#     - 多线程/互斥/信号量以 stub 存在；单线程 SDL1 程序可忽略。
#
#   这样的好处：
#     - SDL1 与 SDL2 两个兼容层完全并行独立、互不依赖。
#     - 减少一次缓冲交换，视频更直接、语义更干净。
#     - 符号冲突面收窄（SDL1 不再桥接 SDL2 公共头）。
#
# When to use / 使用场合
#   - 需要将基于 SDL 的小游戏 / demo 迁移到 TCMIPS 运行。
#   - 用户代码使用 `SDL_SetVideoMode`、`SDL_BlitSurface`、`SDL_Flip`、
#     `SDL_PollEvent`、`SDL_Delay` 等经典 SDL API。
#   - 不希望因为平台差异把 TCMIPS syscall / VRAM 细节写进游戏代码。
#
# What it solves / 解决什么问题
#   - 解决 TCMIPS SDK 缺少 SDL API 的移植空白。
#   - 在 SDL1 单窗口模型和 TCMIPS 物理像素台之间做直接适配。
#   - 把 SDL1 事件队列、键鼠状态、计时器映射到 TCMIPS 原生输入与时钟。
#   - 提供 SDL_RWops 接口，让老资源加载代码无需重写。
#
# Integration / 集成方式
#   This file lives at tcmips/lib/libsdl/CMakeLists.txt and is included via
#   add_subdirectory() from tcmips/lib/CMakeLists.txt.  It depends on:
#   - tcmips_dev      (syscall / keyboard / pixel console helpers)
#   - c               (picolibc; file verbs via BSP glue -> memfs)
#   Public headers are mirrored into the sysroot at <SDL/...> so user programs
#   can `#include <SDL/SDL.h>`.

add_library(tcmips_sdl STATIC
    src/tcmips_sdl_state.c
    src/sdl_init.c
    src/sdl_video.c
    src/sdl_vram.c
    src/sdl_surface.c
    src/sdl_events.c
    src/sdl_timer.c
    src/sdl_stdinc.c
    src/sdl_audio.c
    src/sdl_rwops.c
    src/sdl_joystick.c
    src/sdl_cdrom.c
    src/sdl_stubs.c
)

# Public SDL headers live under include/SDL so user code can #include <SDL/SDL.h>.
target_include_directories(tcmips_sdl PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Internal helpers and TCMIPS device headers.
target_include_directories(tcmips_sdl PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${TCM_SYSROOT}/usr/include>
)

target_link_libraries(tcmips_sdl PRIVATE
    tcmips_dev        # platform helpers (keyboard / console / syscall)
    c                 # picolibc (file verbs via BSP glue -> memfs)
)

# Mirror public headers into the sysroot so user programs see <SDL/...>.
tcm_sync_headers_to_sysroot(tcmips_sdl "${CMAKE_CURRENT_SOURCE_DIR}/include/")
