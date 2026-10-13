# 兼容层移植范式 — TCMIPS

本文档描述向 TCMIPS 移植第三方库 / 兼容层时应遵循的统一范式，归纳自仓库内六个真实范例：
`libsdl`、`libsdl2`、`libcg`、`zlib`、`libstb`、`libfixmath`（后两者为纯计算库代表）。目标是让"移植一个库"从每次重新摸索，变成照着本模板走。

## 1. 什么是"兼容层"

TCMIPS 的兼容层（compatibility layer）不是原样编译上游源码，而是：

> **保留上游的公共 API 头文件（`include/`），再为这些 API 提供一套把调用映射到
> TCMIPS 平台外设/运行时（`tcmips_dev` / `libmemfs` / picolibc）的自研实现。**

这样用户代码可以 `#include <SDL/SDL.h>` 或 `<zlib.h>` 原样编译，而实现背后走的是
TCMIPS 自己的 syscall / VRAM / memfs，而非真实操作系统的服务。

## 2. 判据：什么库值得移植

目标硬件的硬约束决定了移植的价值与可行性：

| 约束 | 含义 | 对移植的推论 |
|---|---|---|
| 低速软件 CPU | 无硬件浮点、无 SIMD | 避免压解压/重计算在运行时反复执行；偏好静态表、轻量算法 |
| `-fno-exceptions` | libc++ 无异常 | 实现里**绝不 throw/try**；失败走返回值 / errno |
| 单核、无线程 | 无 pthread | 拒绝依赖线程的库；SDL 的 mutex/semaphore 用 stub |
| 无音频外设 | 无声音后端 | 音频 API 一律 dummy/stub 实现 |
| 无真实磁盘 | 统一 RAM + memfs | 文件 I/O 走 `open/read/write`（由 BSP 落到 memfs） |

**值得移植的典型特征**：轻量、纯计算或少量 I/O、不依赖异常/线程/浮点硬件、能复用现有 memfs/console/SDL 栈。反例是那些强依赖原生窗口系统、GPU、真实文件系统语义、正厚度系统调用的大框架。

## 3. 标准流程

移植一个库，按下面顺序落地。每一步都有仓库内真实范例可对照。

### 3.1 vendor 溯源注释

在库的 `CMakeLists.txt` 头部写清上游来源，供合规追溯与可复现：

```cmake
# Original upstream repository:
#   https://github.com/<org>/<repo>
# Exact upstream HEAD commit at the time of this import:
#   <full-commit-sha>
# Clone command used:
#   git clone ... https://github.com/<org>/<repo>.git
# To reproduce this exact baseline:
#   cd <repo> && git checkout <full-commit-sha>
#
# License: <SPDX> (Copyright ...).  Full license text is vendored at LICENSE
#   and must be preserved in any redistribution (see root LICENSE 附录).
```

范例：`tcmips/lib/libsdl/CMakeLists.txt`、`tcmips/lib/zlib/CMakeLists.txt`、`tcmips/lib/libcg/CMakeLists.txt`、`tcmips/lib/libstb/CMakeLists.txt`。

### 3.2 目录布局

遵循源码仓库的惯例：

```
tcmips/lib/lib<name>/
  CMakeLists.txt          # 溯源注释 + add_library + 头同步 + link
  include/<Prefix>/...h   # 上游公共头（保留、供 #include <Prefix/...>）
  src/*.c                 # 自研实现（映射到 tcmips_dev / memfs）
  LICENSE                 # 上游 license 原件（必须保留）
```

`include/` 下的 `<Prefix>`（如 `SDL`、`SDL2`、`memfs`、`cg`）决定用户在 sysroot 里看到的
`#include <Prefix/...>` 前缀。

### 3.3 CMakeLists 的三件事（库内）+ 仓库级登记

每个兼容层的 `CMakeLists.txt` 都只做三件事（对照 `libsdl` / `libsdl2` / `libcg` / `zlib` / `libstb` / `libfixmath`）：

```cmake
add_library(tcmips_<name> STATIC
    src/impl1.c
    src/impl2.c
)

# 只依赖它真正用到的东西：tcmips_dev（外设 syscall）、c（picolibc）、
# 必要时 tcmips_memfs / tcmips_zlib。绝不越界依赖。
target_link_libraries(tcmips_<name> PRIVATE tcmips_dev c)

# 把公共头同步进 sysroot，用户才能 #include <Prefix/...>
tcm_sync_headers_to_sysroot(tcmips_<name> "${CMAKE_CURRENT_LIST_DIR}/include/")
```

要点：

- **依赖声明要诚实**：只 link 实现真的用到的库。SDL1 / SDL2 文件 I/O 走 picolibc 的文件层（由
  BSP 落到 memfs），**不直接 link libmemfs** —— 这是刻意的低耦合（见 `libsdl/CMakeLists.txt` 的注释）。
- **头同步**：公共头放 `include/<Prefix>/`，用 `tcm_sync_headers_to_sysroot` 同步。若上游头是
  平铺的（没有 `Prefix/` 目录），用 `DEST_PREFIX` 参数补前缀（范例：`libcg` 的
  `tcm_sync_headers_to_sysroot(tcmips_cg "src" DEST_PREFIX cg)`）。
- **资源打包**：若库自带只读资源（字体、配置），用 `STATIC_LIBRARY_OPTIONS` 的
  `-tcm-memfs=<dir>` 打包进镜像（范例：`tcmips_dev` 的 unifont 字体、`tcmips_bsp` 的 logo）。

#### 3.3.1 仓库级登记（易漏，必须做）

兼容层的 `CMakeLists.txt` 本身不足以让新库"被构建、且不乱链接、且能被用户链上"，还要登记三处：

1. **`tcmips/CMakeLists.txt` 的 `TCMIPS_CORE_TARGETS`** —— 追加 `tcmips_<name>`。
   `TCMIPS_CORE_ALL` 只依赖这份列表，未登记的库**不参与 `ALL` 构建**（`add_subdirectory`
   引入 ≠ 参与构建），打包时也不会产出 `.bc`。
2. **`toolchains/cmake/tcmips.toolchain.cmake` 的 `TCM_EXCLUDED_LIBS`** —— 追加
   `"libtcmips_<name>.bc"`。该名单是"按需显式链接"的可选库白名单；未登记的 `.bc` 会被
   toolchain **强制链接进每一个 guest 工程**（即使用户根本不用它）。
3. **同文件 "Optional targets" 段落** —— 追加一段 `add_library(tcmips_<name> INTERFACE
   IMPORTED GLOBAL)` 并设 `INTERFACE_LINK_LIBRARIES` 指向其 `.bc`，把该库暴露为可链接目标。

**第 2、3 步是必须成对的一对**：第 2 步把它挡在自动链接之外，第 3 步才让用户"想链就能链"。
只做第 2 步而不做第 3 步，用户按文档写 `target_link_libraries(<tgt> PRIVATE tcmips_<name>)`
会因目标不存在而配置失败。三者的对照：

| 登记点 | 作用 | 漏做的后果 |
|---|---|---|
| `TCMIPS_CORE_TARGETS` | 参与 `ALL` 构建 | 永不构建，`.bc` 缺失 |
| `TCM_EXCLUDED_LIBS` | 不自动链接 | 污染所有 guest 工程 |
| "Optional targets" IMPORTED 目标 | 可按需显式链接 | 用户无法链接该库 |

范例：`tcmips_cg` / `tcmips_sdl` / `tcmips_sdl2` / `tcmips_stb` / `tcmips_fixmath`
五个可选库均已按此三处登记齐全。

### 3.4 实现映射

`src/*.c` 里把上游 API 的函数体，映射到 TCMIPS 平台接口：

| 上游关注点 | 映射到 TCMIPS |
|---|---|
| 键盘输入 | `tcm_syscall_read_keyboard()`（见 `sdl_events.c`） |
| 计时 | `tcm_syscall_get_timestamp*()`（秒/毫/微/纳秒，见 `sdl_timer.c`） |
| 视频 / 帧缓冲 | `tcm_pixel_console_init()` 拿 VRAM 指针，直接写（见 `sdl_vram.c`） |
| 文本输出 | `tcm_text_console_*` |
| 文件 I/O | 标准 `open/read/write`（BSP 落到 memfs，**不要**调用 memfs 内部） |
| 音频 / 线程 / 互斥 | stub / dummy（无后端，返回空或错误） |

原则：**实现里只依赖 `tcmips_dev` 的公共接口 + picolibc 标准库，不触碰 memfs 内部约定**，
这是保持低耦合、方便替换的关键。

### 3.5 接入回归测试

每个兼容层应带一个 `AUTOTEST` 目标，验证实现正确性：

```
test/<模块>/test_<name>_autotest.c     # [SELF] <name> PASS/FAIL 断言
test/<模块>/CMakeLists.txt             # add_executable + link tcmips_core + tcmips_<name>
```

并在 `test/CMakeLists.txt` 的 `add_subdirectory(...)` 与 `AUTOTEST_TARGETS` 里登记。
测试方法遵循 [`AUTOTEST.md`](AUTOTEST.md)：判断标准、退出码区间、`-mode=fast` 访存校验。

## 4. 移植时的约束清单（雷区）

- **异常不可用**：`-fno-exceptions` 交叉了 libc++，任何 throw/try 都会 `abort()`。
  失败路径一律走返回值 / `errno` / `error_code` 重载。
- **无线程**：目标单核；SDL 的 mutex/semaphore 只能 stub。
- **无音频**：音频 API 一律 dummy。
- **低速 CPU 性能**：运行时解压/重计算会在真实硬件上形成明显卡顿（压缩字体已实测不适用，
  因为目标板卡时钟低），优先静态表、预计算、不做热路径解压。
- **C++ 用 C++17**：libc++ 14 对 C++20 支持有限。
- **头文件先 include 正确头再调接口**：工具链开 `-Werror=implicit-function-declaration`。
- **picolibc ≠ glibc**：`getline` EOF 语义等有差异，写断言前先查 picolibc 实际实现。

## 5. 范例索引

| 库 | 关键特性 | 参照文件 |
|---|---|---|
| `libsdl` | SDL1 直驱 tcmips_dev（不桥接 SDL2） | `tcmips/lib/libsdl/` |
| `libsdl2` | SDL2 公共头 + 自研实现 | `tcmips/lib/libsdl2/` |
| `libcg` | 2D 图形；`DEST_PREFIX` 处理平铺头 | `tcmips/lib/libcg/` |
| `zlib` | 纯算法库；`tcm_sync_headers_to_sysroot` 同步根头 | `tcmips/lib/zlib/` |
| `libstb` | 纯计算库（PNG/缩放/字栅化/数组/哈希/格式化/矩形打包）；头文件平铺，无 `Prefix/` 子目录 | `tcmips/lib/libstb/` |
| `libfixmath` | Q16.16 定点数学库（乘除/平方根/三角/指数对数）；纯计算、上游源码原样拷贝、零修改 | `tcmips/lib/libfixmath/` |
| `libmemfs` | ABI 头 `memfs_abi.h`（链接器契约）与运行时头 `memfs.h` 分离 | `tcmips/lib/libmemfs/` |
