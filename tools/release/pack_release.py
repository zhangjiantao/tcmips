#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""TCMIPS Release 打包脚本（tcmips-devkit）——只做「收集 + 压缩」，不做任何编译。

前置产物（模拟器 exe、demo .tcm、sysroot 等）须由用户先行构建好；本脚本不触发
cmake / 编译，某包所需文件缺失时会明确报错并跳过该包（其余包照常产出）。

打包两个发行产物（命名 tcmips-<类型>[-<host>]-<version>.{tar.gz,zip}）：

  [1] 工具链包  tcmips-toolchains-<host>-<version>.{tar.gz,zip}（SDK，二次开发 + 尝鲜演示）
      - toolchains/llvm/bin/*                     （clang / clang++ / llvm-link-tcmips）
      - toolchains/llvm/lib/clang/.../include      （clang 内置头，编译必需）
      - toolchains/cmake/tcmips.toolchain.cmake   （交叉 toolchain 入口）
      - toolchains/sysroot/                       （sysroot）
      - emulator/TCMIPSEmulator(.exe)              （宿主模拟器，与 toolchains/ 平级）
      - emulator/demo/*.tcm                        （预编译 demo 镜像）
      - LICENSE / README.md（自动生成）

  [2] 存档包    tcmips-gamesave-<version>.{tar.gz,zip}（纯资产，跨平台，无 host 后缀）
      - gamesave/                                 （architecture + foundry 全部存档文件）
      - FILELIST.txt                             （相对路径 | 字节数 | md5 完整性清单）
      - LICENSE / README.md（自动生成）

strip：llvm/bin 内的可执行文件支持打包时 strip——strip 的是临时副本，不改动源文件；
  优先级：--strip-tool 显式指定 > PATH 中的 strip > 找不到则跳过并警告。

用法示例：
  python tools/pack_release.py --version 2.1.1                     # 两个包全打，输出 <repo>/dist
  python tools/pack_release.py --toolchains-only --version 2.1.1   # 只打工具链包（含模拟器与 demo）
  python tools/pack_release.py --gamesave-only   --version 2.1.1   # 只打游戏存档包
  python tools/pack_release.py --version 2.1.1 --format zip        # 输出 zip 而非 tar.gz
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

# 强制 UTF-8 输出，避免 Windows GBK 控制台打印 ✓ 等非 ASCII 字符时崩溃。
# Python 3.7+ 支持；POSIX 下通常已是 UTF-8，此设置无害。
if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# --------------------------------------------------------------------------- #
# 路径规划
# --------------------------------------------------------------------------- #

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LLVM_REL = os.path.join("toolchains", "llvm")
CMAKE_REL = os.path.join("toolchains", "cmake")
SYSROOT_REL = os.path.join("toolchains", "sysroot")

LICENSE_FILENAME = "LICENSE"
GAMESAVE_REL = "gamesave"

NOTE_TEXT = "由 pack_release.py 生成。"


def host_suffix() -> str:
    """生成包名的宿主平台后缀（如 x86_64-mingw64 / aarch64-darwin / x86_64-linux）。"""
    raw = platform.machine().lower() or "unknown"
    machine = {"x86_64": "x86_64", "amd64": "x86_64", "i386": "i686",
               "i686": "i686", "intel64": "x86_64", "aarch64": "aarch64",
               "arm64": "aarch64", "arm": "arm", "riscv64": "riscv64",
               "ppc64le": "ppc64le"}.get(raw, raw)

    plat = sys.platform  # 'win32' / 'darwin' / 'linux' / etc.
    if plat == "win32" or os.name == "nt":
        vendor = "mingw64" if machine in ("x86_64", "i686") else "mingw"
        return f"{machine}-{vendor}"
    if plat == "darwin":
        return f"{machine}-darwin"
    if plat == "linux" or os.name == "posix":
        return f"{machine}-linux"
    return f"{machine}-platform"


# --------------------------------------------------------------------------- #
# 收集器：返回 (打包内相对路径 -> 源绝对路径) 的映射
# --------------------------------------------------------------------------- #

def _add_file(items: dict[str, str], rel_in_archive: str, src: str) -> None:
    if not os.path.isfile(src):
        raise FileNotFoundError(src)
    items[rel_in_archive] = src


# --------------------------------------------------------------------------- #
# 垃圾文件过滤：打包前剔除 .DS_Store 之类的系统/编辑器杂物
# --------------------------------------------------------------------------- #

_JUNK_FILENAMES = frozenset({
    ".DS_Store",       # macOS Finder
    "Thumbs.db",       # Windows 缩略图缓存
    "Desktop.ini",     # Windows 文件夹元数据
    ".directory",      # 资源管理器
})

_JUNK_PATTERNS = (
    "*~",          # 编辑器备份
    "*.bak",
    "*.tmp",
    "*.swp",
    "*.swo",
)


def _is_junk_name(name: str) -> bool:
    """返回 True 表示该文件/目录名属于应被剔除的杂物（精确名或通配模式）。"""
    if name in _JUNK_FILENAMES:
        return True
    return any(fnmatch.fnmatch(name, pattern) for pattern in _JUNK_PATTERNS)


def _add_tree(items: dict[str, str], rel_prefix: str, src_dir: str) -> None:
    if not os.path.isdir(src_dir):
        raise FileNotFoundError(f"目录不存在: {src_dir}")
    for root, dirs, files in os.walk(src_dir):
        # 剪枝垃圾目录，避免递归进入（__pycache__ / .git 等）
        dirs[:] = [d for d in dirs if not _is_junk_name(d)]
        for f in files:
            if _is_junk_name(f):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, src_dir)
            items[os.path.join(rel_prefix, rel).replace(os.sep, "/")] = full


def detect_strip_tool(explicit: str | None) -> str | None:
    """解析 strip 工具路径：--strip-tool 显式指定 > PATH 中的 strip > None（跳过 strip）。"""
    if explicit:
        if os.path.isfile(explicit):
            print(f"  [strip] 使用显式工具: {explicit}")
            return explicit
        print(f"  [warn] --strip-tool 指定的路径不存在，忽略并回退到自动探测: {explicit}")
    found = shutil.which("strip")
    if found:
        print(f"  [strip] 使用 PATH 工具: {found}")
        return found
    print("  [warn] 未找到 strip 工具，跳过 strip（包内可执行文件体积较大但无碍功能）。")
    return None


def _read_cmake_cache_value(repo_root: str, key: str) -> str | None:
    """从 <repo>/cmake-build-release/CMakeCache.txt 读取单个 key 的值。

    仅用于获取 cmake / ninja 的绝对路径（CMAKE_COMMAND / CMAKE_MAKE_PROGRAM），
    以便打包时把宿主机通用构建工具一并收进工具链包。找不到文件或 key 返回 None。
    """
    cache = os.path.join(repo_root, "cmake-build-release", "CMakeCache.txt")
    if not os.path.isfile(cache):
        return None
    with open(cache, encoding="utf8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            # 形如 "CMAKE_COMMAND:INTERNAL=D:/path/to/cmake.exe"；value 可能含盘符冒号，
            # 故以 "=" 首次切分取右半，而不是按 ":" 切。
            if line.startswith(key + ":") or line.startswith(key + "="):
                _, _, rest = line.partition("=")
                if rest:
                    return rest.strip()
    return None


def _collect_host_tools(items: dict[str, str], repo_root: str) -> None:
    """把宿主机的 cmake（bin + share）与 ninja 收进工具链包（Windows 专用）。

    从 cmake-build-release/CMakeCache.txt 读取 CMAKE_COMMAND / CMAKE_MAKE_PROGRAM
    的绝对路径；cmake 的 share/ 与 bin/ 同级（CMAKE_COMMAND 的 bin 上级目录下）。
    仅当前运行平台为 Windows 时收集，其余平台跳过（cmake/ninja 是平台相关的可执行
    文件，跨平台收集无意义）。任一缺失仅打印警告并跳过，不视为致命错误。
    """
    if os.name != "nt":
        print("  [host-tools] 非 Windows 平台，跳过 cmake/ninja 收集。")
        return

    cmake_cmd = _read_cmake_cache_value(repo_root, "CMAKE_COMMAND")
    ninja_prog = _read_cmake_cache_value(repo_root, "CMAKE_MAKE_PROGRAM")

    # --- ninja：单文件，原样收进包内 toolchains/host/ninja/ ---
    if ninja_prog and os.path.isfile(ninja_prog):
        items["toolchains/host/ninja/" + os.path.basename(ninja_prog)] = ninja_prog
        print(f"  [host-tools] ninja: {ninja_prog}")
    else:
        print("  [warn] 未从 CMakeCache 找到 ninja（CMAKE_MAKE_PROGRAM），跳过 ninja 收集。")

    # --- cmake：收 bin/ 整目录 + 同级 share/ 整目录，到 toolchains/host/cmake/ ---
    if cmake_cmd and os.path.isfile(cmake_cmd):
        cmake_bin = os.path.dirname(cmake_cmd)                 # .../cmake/win/x64/bin
        cmake_root = os.path.dirname(cmake_bin)                # .../cmake/win/x64
        cmake_share = os.path.join(cmake_root, "share")
        if os.path.isdir(cmake_share):
            _add_tree(items, "toolchains/host/cmake/bin", cmake_bin)
            _add_tree(items, "toolchains/host/cmake/share", cmake_share)
            print(f"  [host-tools] cmake: bin + share 来自 {cmake_root}")
        else:
            print(f"  [warn] cmake 的 share 目录不存在（期望 {cmake_share}），"
                  f"只收集 bin 可能无法 configure。")
            _add_tree(items, "toolchains/host/cmake/bin", cmake_bin)
    else:
        print("  [warn] 未从 CMakeCache 找到 cmake（CMAKE_COMMAND），跳过 cmake 收集。")


def _collect_llvm_bin(items: dict[str, str], bin_dir: str, strip_tool: str | None) -> None:
    """把 llvm/bin 下的文件加入包内。

    strip 有副作用（就地删符号），为不污染本地原始工具链：需要 strip 时先把文件
    复制到临时目录，对副本 strip 后把副本收进包；strip 失败则收未 strip 的原文件。
    """
    need_strip = bool(strip_tool) and os.path.isfile(strip_tool)
    for f in sorted(os.listdir(bin_dir)):
        if _is_junk_name(f):
            continue
        src = os.path.join(bin_dir, f)
        if not os.path.isfile(src):
            continue
        rel = "toolchains/llvm/bin/" + f
        if need_strip and os.path.splitext(f)[1] in ("", ".exe"):
            tmpdir = os.path.join(tempfile.gettempdir(), "tcmips_strip")
            os.makedirs(tmpdir, exist_ok=True)
            dst = os.path.join(tmpdir, f)
            shutil.copy2(src, dst)
            try:
                subprocess.run([strip_tool, dst], check=True)
                print(f"  [strip] {f} (copy)")
            except subprocess.CalledProcessError as e:
                print(f"  [warn] strip 失败，改用未 strip 的原文件: {f}: {e}")
                dst = src
            items[rel] = dst
        else:
            items[rel] = src


def collect_sdk(repo_root: str, strip_tool: str | None,
                emulator_exe: str | None = None) -> dict[str, str]:
    """收集工具链（SDK）包内容（含宿主模拟器 + 演示镜像）。缺文件抛 FileNotFoundError / RuntimeError。"""
    items: dict[str, str] = {}

    # --- llvm/bin：clang / clang++ / llvm-link-tcmips 必须在，缺失直接报错 ---
    bin_dir = os.path.join(repo_root, LLVM_REL, "bin")
    suffix = ".exe" if os.name == "nt" else ""
    missing = [n for n in ("clang" + suffix, "clang++" + suffix,
                           "llvm-link-tcmips" + suffix)
               if not os.path.isfile(os.path.join(bin_dir, n))]
    if missing:
        raise RuntimeError(
            f"{bin_dir} 下缺失: {', '.join(missing)}。请先补齐 toolchains/llvm 再打包。")
    _collect_llvm_bin(items, bin_dir, strip_tool)

    # --- clang 内置头（编译必需） ---
    _add_tree(items, "toolchains/llvm/lib/clang",
              os.path.join(repo_root, LLVM_REL, "lib", "clang"))
    # --- toolchain cmake 入口 ---
    _add_file(items, "toolchains/cmake/tcmips.toolchain.cmake",
              os.path.join(repo_root, CMAKE_REL, "tcmips.toolchain.cmake"))
    # --- sysroot ---
    _add_tree(items, "toolchains/sysroot", os.path.join(repo_root, SYSROOT_REL))

    # --- 宿主机通用构建工具：cmake(bin+share) + ninja（仅 Windows，见 _collect_host_tools） ---
    _collect_host_tools(items, repo_root)

    # --- 宿主模拟器：合并进工具链包，与 toolchains/ 平级放到 emulator/ ---
    _collect_emulator_exe(items, repo_root, emulator_exe)

    # --- 演示镜像：demo .tcm 由 CMake 输出到构建目录（cmake-build-release/demo，见
    #     demo/CMakeLists.txt 的 RUNTIME_OUTPUT_DIRECTORY），此处二次收集 .tcm，
    #     与模拟器一起平级放入 emulator/demo/。链接器还会生成 <name>.tcm.elf 调试
    #     中间产物，收集时只挑 .tcm，忽略 .tcm.elf（不碰磁盘源文件）。 ---
    _collect_demo_tcm(items, repo_root, os.path.join("emulator", "demo"))

    # --- 示例工程：最小 C 模板（examples），供用户解压后直接编译上手 ---
    _add_tree(items, "examples", os.path.join(repo_root, "examples"))

    # --- LICENSE（合规必需，含第三方声明） ---
    _add_file(items, LICENSE_FILENAME, os.path.join(repo_root, LICENSE_FILENAME))

    return items


def _collect_demo_tcm(items: dict[str, str], repo_root: str, rel_prefix: str) -> None:
    """收集预编译 demo .tcm（cmake-build-release/demo 下，TCMIPS_DEMO_ALL 的输出）。"""
    demo_dir = os.path.join(repo_root, "cmake-build-release", "demo")
    if not os.path.isdir(demo_dir):
        raise FileNotFoundError(
            f"未找到 demo 输出目录 {demo_dir}。请先构建 demo（TCMIPS_DEMO_ALL）。")
    tcm_files = sorted(f for f in os.listdir(demo_dir)
                       if f.endswith(".tcm") and os.path.isfile(os.path.join(demo_dir, f)))
    if not tcm_files:
        raise FileNotFoundError(
            f"未找到预编译 demo .tcm（期望 {demo_dir} 下有 *.tcm）。请先构建 demo。")
    for f in tcm_files:
        items[os.path.join(rel_prefix, f)] = os.path.join(demo_dir, f)


def _collect_emulator_exe(items: dict[str, str], repo_root: str,
                          emulator_exe: str | None) -> None:
    """把宿主模拟器可执行文件追加到包内 emulator/（与 toolchains/ 平级）。"""
    exe_name = "TCMIPSEmulator.exe" if os.name == "nt" else "TCMIPSEmulator"
    if emulator_exe:
        if not os.path.isfile(emulator_exe):
            raise FileNotFoundError(f"--emulator-exe 指定的文件不存在: {emulator_exe}")
        emu_path = emulator_exe
    else:
        cand = os.path.join(repo_root, "emulator", "cmake-build-release", exe_name)
        emu_path = cand if os.path.isfile(cand) else None
    if not emu_path:
        raise FileNotFoundError(
            f"未找到宿主模拟器 {exe_name}（默认位置 "
            f"{os.path.join(repo_root, 'emulator', 'cmake-build-release', exe_name)}，"
            f"可用 --emulator-exe 显式指定）。请先构建模拟器。")
    rel = os.path.join("emulator",
                       "TCMIPSEmulator" + (".exe" if emu_path.endswith(".exe") else ""))
    items[rel] = emu_path


def collect_gamesave(repo_root: str) -> dict[str, str]:
    """收集游戏存档包内容：gamesave/ 全部文件 + FILELIST.txt 清单（纯资产，无前置构建）。"""
    gamesave_dir = os.path.join(repo_root, GAMESAVE_REL)
    if not os.path.isdir(gamesave_dir):
        raise FileNotFoundError(f"目录不存在: {gamesave_dir}")

    items: dict[str, str] = {}
    _add_tree(items, "gamesave", gamesave_dir)

    # FILELIST.txt：gamesave/ 下每个文件的 相对路径 / 字节数 / md5
    lines = ["# TCMIPS GameSave 文件清单", "# 格式: <相对路径> | <字节数> | <md5>", ""]
    prefix = gamesave_dir.rstrip(os.sep) + os.sep
    for root, _dirs, files in sorted(os.walk(gamesave_dir)):
        for f in sorted(files):
            if _is_junk_name(f):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, prefix).replace(os.sep, "/")
            size = os.path.getsize(full)
            md5 = hashlib.md5(open(full, "rb").read()).hexdigest()
            lines.append(f"gamesave/{rel} | {size} | {md5}")
    lines.append("")
    items["FILELIST.txt"] = write_temp_text("\n".join(lines))

    # LICENSE
    _add_file(items, LICENSE_FILENAME, os.path.join(repo_root, LICENSE_FILENAME))

    return items


# --------------------------------------------------------------------------- #
# 压缩
# --------------------------------------------------------------------------- #

def _add_to_archive(archive, items: dict[str, str]) -> None:
    """把 {包内相对路径 -> 源绝对路径} 写进已打开的归档。包内路径统一用 /。"""
    for rel, src in sorted(items.items()):
        rel_posix = rel.replace(os.sep, "/")
        if isinstance(archive, tarfile.TarFile):
            archive.add(src, arcname=rel_posix)
        elif isinstance(archive, zipfile.ZipFile):
            zinfo = zipfile.ZipInfo.from_file(src, rel_posix)
            zinfo.compress_type = zipfile.ZIP_DEFLATED
            with open(src, "rb") as fh:
                archive.writestr(zinfo, fh.read())
        else:
            raise TypeError(f"不支持归档类型: {type(archive)}")


def make_archive(out_path: str, fmt: str, items: dict[str, str]) -> None:
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    if fmt in ("tar.gz", "tgz"):
        with tarfile.open(out_path, "w:gz") as tf:
            _add_to_archive(tf, items)
    elif fmt == "zip":
        with zipfile.ZipFile(out_path, "w") as zf:
            _add_to_archive(zf, items)
    else:
        raise ValueError(f"不支持的格式: {fmt}（支持 tar.gz / tgz / zip）")


_TEMP_FILES: list[str] = []  # 脚本运行期间生成的临时文件，main 结束时统一删除


def write_temp_text(text: str) -> str:
    """把生成的文本（README / 清单）写入临时文件并返回路径（由 main 统一清理）。"""
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False,
                                     encoding="utf-8") as tf:
        tf.write(text)
        path = tf.name
    _TEMP_FILES.append(path)
    return path


# --------------------------------------------------------------------------- #
# README（自动生成）
# --------------------------------------------------------------------------- #

SDK_README_TEMPLATE = """\
# TCMIPS SDK {version}

面向 TCMIPS 架构的 C/C++ 交叉编译工具链（`mipsel-unknown-elf`，mips32r2，
soft-float，abi=32），并附带宿主机 TCMIPS 模拟器与预编译演示镜像。解压后即可
编译出自己的 `.tcm` 可执行文件，再用附带模拟器运行，或在游戏 File Loader 里加载。

## 目录结构

```
toolchains/
  cmake/tcmips.toolchain.cmake   交叉编译 CMake toolchain 入口（必须用它）
  host/cmake/                    附带宿主机 cmake（bin/ + share/，Windows 版）
    bin/cmake.exe
    share/cmake-*/               CMake 模块 / 模板（configure 必需）
  host/ninja/                    附带宿主机 ninja（单文件）
    ninja.exe
  llvm/bin/                      改造过的 clang / clang++ / llvm-link-tcmips
  llvm/lib/clang/.../include      clang 内置标准头
  sysroot/                        sysroot（标准库 / 兼容层位码库 .bc）
emulator/
  TCMIPSEmulator                 宿主模拟器（宿主侧可执行文件）
  demo/*.tcm                     预编译演示镜像
examples/
  CMakeLists.txt + main.c        最小 C 示例工程（printf "Hello, world!"）
```

## 快速上手

### 环境准备：cmake / ninja

本 SDK 包内（Windows 版）已附带宿主机通用构建工具，可直接使用，无需单独安装：

- `toolchains/host/cmake/bin/cmake.exe`（连同 `bin/`、`share/` 依赖）
- `toolchains/host/ninja/ninja.exe`

若你的包内未附带（例如其他平台版本），请按下面方式自行安装后再编译：

- **Windows**
  - CMake：<https://cmake.org/download/> —— 下载 Windows x64 安装包，安装时勾选
    “Add CMake to the system PATH”。
  - Ninja：<https://github.com/ninja-build/ninja/releases> —— 下载 `ninja-win.zip`，
    解压后把 `ninja.exe` 所在目录加入 PATH。
- **macOS**：`brew install cmake ninja`
- **Linux**：Debian 系 `apt install cmake ninja-build`；RHEL 系 `dnf install cmake ninja-build`

安装后执行 `cmake --version`、`ninja --version` 确认可用。

### 编译示例工程

`examples/` 是最小示例工程，其 CMakeLists.txt 已自动定位工具链，解压后进入目录
直接编译即可：

```text
cd examples
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物为 `build/hello.tcm`，运行方式见「构建与运行」。

## 构建与运行

### 搭建你的工程

在你的 C/C++ 工程里这样构建：

```text
cmake -G Ninja -S . -B build \\
  -DCMAKE_BUILD_TYPE=Release \\
  -DCMAKE_TOOLCHAIN_FILE=<解压根>/toolchains/cmake/tcmips.toolchain.cmake
cmake --build build
```

### 用模拟器运行

```text
<解压根>/emulator/TCMIPSEmulator <image.tcm>             # 打开窗口运行
<解压根>/emulator/TCMIPSEmulator <image.tcm> -hold=0     # 程序退出后自动退出
<解压根>/emulator/TCMIPSEmulator emulator/demo/<demo名>.tcm
```

生成的 `.tcm` 也可直接在游戏里的 File Loader 加载，无需模拟器。

### 模拟器参数

| 参数 | 说明 |
| --- | --- |
| `<image.tcm>` | 要加载的镜像 |
| `-fps=<1-120>` | 目标帧率，默认 60，0 = 不限 |
| `-mode=<mode>` | 解释器：jit（默认）/ fast / debug（debug 支持 GDB 连接） |

## SDK 提供的库

### 自动注入（无需 target_link_libraries）

| 库 | 说明 |
| --- | --- |
| `libc` (picolibc) | C 标准库（stdio / stdlib / string / math 等） |
| `libc++` / `libc++abi` | C++ 标准库（`-fno-exceptions`，推荐 `-std=c++17`） |
| `compiler-rt` | 软浮点与 64 位整数运算 |
| `tcmips_memfs` | 内存文件系统（链接时烧入文件内容，运行时标准文件操作读取） |
| `tcmips_dev` | 外设 syscall 接口（console / keyboard / seven-segment / palette / semihost stdio） |

### 需手动链接（显式 target_link_libraries）

| 库 | 说明 | 建议 |
| --- | --- | --- |
| `tcmips_sdl` | SDL1 兼容层（无音频后端） | 推荐 |
| `tcmips_sdl2` | SDL2 兼容层（无音频后端） | 完成度低，不推荐 |
| `tcmips_cg` | 轻量 2D 图形库 | 不推荐 |

```text
target_link_libraries(my_app PRIVATE tcmips_sdl)
```

## 关键限制

- 无 C++ 异常（`throw` / `catch` 会转为 `abort()`）
- 无多线程
- 无音频（SDL 音频后端为 stub）
- C++17 推荐（libc++ 仅部分支持 C++20）

## 说明

{note}

合规：本包内含第三方组件，各自许可见 LICENSE（THIRD-PARTY NOTICES 附录）。
demo 目录内的 Doom / SDLPAL / umac(部分) 等含 GPL 组件，其版权条款独立，不受本
仓库 MIT 许可覆盖；ROM / 素材若需版权文件（如 Mac ROM），请自行获取，不得随包分发。
分发本包时须原样保留 LICENSE。
"""

SDK_AGENTS_TEMPLATE = """\
# TCMIPS SDK — AI 协作约定

这是面向 TCMIPS 架构的 C/C++ 交叉编译工具链包（版本 {version}）。你正在协助用户
用本 SDK 编写并编译运行在 TCMIPS CPU 上的 `.tcm` 程序。

## 协作语言约定

- **思考过程用中文**：思考、推理、规划、说明等一律简体中文。
- **输出优先中文**：tcmips 支持 UTF-8 中文 std 输出，程序输出信息优先用中文；仅对
  接只认英文的第三方/协议时才用英文。源码字符串可直接写 UTF-8 中文。

## 构建前置：cmake / ninja

本 SDK 包内已附带宿主机通用构建工具（Windows 平台），统一放在 `toolchains/host/` 下：

- `{prefix}toolchains/host/cmake/bin/cmake.exe`（及其 `bin/`、`share/` 依赖）
- `{prefix}toolchains/host/ninja/ninja.exe`

定位与使用规则（务必照做，不要盲目 `where`/`which` 或扫描全盘）：

1. **优先使用包内的** `cmake` 与 `ninja`（上面两个路径）。
2. 若包内缺失（例如用户拿到的是非 Windows 包、或未来版本未内置），再探测系统：
   - `cmake --version`、`ninja --version` 是否已在 PATH；
   - Windows 常见目录 `C:\\Program Files\\CMake\\bin\\cmake.exe` 等。
3. 都找不到时，**停止编译，明确询问用户**「未找到 cmake/ninja，请提供它们的绝对
   路径，或安装后加入 PATH」，不要绕过或臆造替代命令。

如需自行安装（包内缺失时）：

- CMake：https://cmake.org/download/ （Windows 用 x64 installer，勾选 “Add CMake to the system PATH”）
- Ninja：https://github.com/ninja-build/ninja/releases （下载 `ninja-win.zip`，解压后加入 PATH）
- macOS：`brew install cmake ninja`
- Linux：`apt install cmake ninja-build`（Debian 系）或 `dnf install cmake ninja-build`（RHEL 系）

## 编译

以下路径相对本目录（即本 AGENTS.md 所在目录）。用户工程用 CMake + Ninja，
必须指定本工具链文件：

```text
cmake -G Ninja -S . -B build \\
  -DCMAKE_BUILD_TYPE=Release \\
  -DCMAKE_TOOLCHAIN_FILE={prefix}toolchains/cmake/tcmips.toolchain.cmake
cmake --build build
```

产物为 `<target>.tcm`，用模拟器运行：

```text
{prefix}emulator/TCMIPSEmulator <image>.tcm [-hold=0]
```

或在 `CMakeLists.txt` 中设置 `CMAKE_TOOLCHAIN_FILE` ，应注意在`project()`命令前设置，否则不生效。

## 运行与结果判定（自动化脚本注意）

- 无交互自动化运行时加 `-hold=0 -mode=fast`，guest 退出后模拟器自动退出。
  必须用 `-mode=fast`（而非默认 `-mode=jit`）：JIT 数据访存不经过 `memory.h` 访问器，
  无法触发非法访存校验；只有 `fast` / `debug` 模式才校验。
- guest 的 `printf` 输出转发到宿主 stdout，退出码经 semihost 写 stderr 为
  `!! TCMIPS EMULATOR EXIT, code=N !!`。
- 模拟器进程退出码划分为两个互不重叠区间，需区分：
  - **guest 退出码 `[0, 127]`**：guest 的 `_exit()` 会 `& 0x7F`，无论如何都到不了
    ≥128 的值；`0` = 成功，`1~127` = guest 失败（`BREAK(0x3ff)` 的 `$a1`）。
  - **模拟器错误码 `[128, 255]`**：
    - `130`：用户手动关窗（`STOPPED BY USER`，非失败）；
    - `131`：guest 非法访存（`MEMORY FAULT`，越界/guard page，应视为失败）；
    - `132~135`：用法/stdin/加载/内部错误（环境问题）。
  - 判定以「退出码区间 + stderr 文案」为准：`STOPPED BY USER`(130) 与
    `MEMORY FAULT`(131) 分别区别于 guest 的 `EXIT, code=N`。

## 库链接规则

- 以下库由工具链自动注入，无需 target_link_libraries：
  libc (picolibc)、libc++ / libc++abi、compiler-rt、tcmips_memfs、tcmips_dev。
- 以下库需手动 target_link_libraries：
  tcmips_sdl（推荐）、tcmips_sdl2（完成度低，不推荐）、tcmips_cg。

## 关键限制（写程序前务必确认）

- 无 C++ 异常（throw / catch 会转为 abort()），可能失败的操作走返回值 / errno。
- 无多线程、无音频（SDL 音频后端为 stub）。
- C++ 推荐 -std=c++17（libc++ 仅部分支持 C++20）。

完整说明见 [README.md]({readme})。
"""

GAMESAVE_README_TEMPLATE = """\
TCMIPS GameSave {version}
=========================
TCMIPS CPU 架构的 Turing Complete 游戏存档（schematics）。

内容
----
  gamesave/
    schematics/architecture/  两个完整 CPU 架构存档（TCMIPS / TCMIPS-LITE）
                              每个含 circuit.data + spec.isa + sandbox/ 初稿
    schematics/foundry/       分块积木（ALU / ALUC / DEV / ID / MISC / SYSCALL 等）
  FILELIST.txt                包内全部文件的 相对路径 | 字节数 | md5（用于完整性校验）
  LICENSE                     第三方声明（本包主要为自研架构内容；如含第三方须保留声明）

导入方法（Turing Complete 沙盘）
--------------------------------
  将schematics目录复制至游戏存档目录即可。
  完整说明见仓库文档 docs/ 与仓库根 README。

说明
----
{note}
ROM 镜像等受版权保护的素材不在本包内，需自行准备，不得随包分发。
"""


# --------------------------------------------------------------------------- #
# 主流程
# --------------------------------------------------------------------------- #

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="pack_release.py",
        description="收集并打包 TCMIPS 发行产物（工具链含模拟器 / 游戏存档）。"
                    "只做收集与压缩，不编译；前置产物须已构建好。",
    )
    p.add_argument("--version", required=True, help="版本号，用于压缩包命名（必填，如 2.1.1）")
    p.add_argument("--repo-root", default=REPO_ROOT, help=f"仓库根（默认 {REPO_ROOT}）")
    p.add_argument("--emulator-exe", default=None,
                   help="显式指定模拟器 exe 路径（默认 emulator/cmake-build-release/TCMIPSEmulator(.exe)）")
    p.add_argument("--format", choices=["tar.gz", "tgz", "zip"], default="zip",
                   help="压缩格式（默认 zip）")
    p.add_argument("--out", default=os.path.join(REPO_ROOT, "dist"),
                   help="压缩包输出目录（默认 <repo>/dist）")
    p.add_argument("--host", default=None,
                   help="工具链包的平台后缀（如 x86_64-mingw64）。默认按当前平台自动生成。")
    p.add_argument("--strip-tool", default=None,
                   help="可选：strip 工具路径（显式覆盖自动探测）。未提供则探测 PATH，找不到就跳过 strip。")
    g = p.add_mutually_exclusive_group()
    g.add_argument("--toolchains-only", action="store_true", help="只打工具链包（含模拟器与 demo）")
    g.add_argument("--gamesave-only", action="store_true", help="只打游戏存档包（纯资产）")
    return p


def main() -> int:
    args = build_parser().parse_args()
    # 相对路径一律转绝对路径：使脚本无论从哪个工作目录（cwd）启动都能正确定位。
    repo_root = os.path.abspath(args.repo_root)
    out_dir = os.path.abspath(args.out) if args.out else os.path.join(repo_root, "dist")
    out_fmt = args.format
    version = args.version.strip()
    if not version:
        print("\n[错误] --version 不能为空，请提供版本号（如 2.1.1）。")
        return 1
    host = args.host or host_suffix()
    ext = ".tar.gz" if out_fmt in ("tar.gz", "tgz") else ".zip"
    os.makedirs(out_dir, exist_ok=True)

    # (标签, 收集函数, 输出文件名, README 模板)
    packages = []
    if not args.gamesave_only:
        packages.append((
            "工具链包（含模拟器）",
            lambda: collect_sdk(repo_root, detect_strip_tool(args.strip_tool),
                                emulator_exe=args.emulator_exe),
            f"tcmips-toolchains-{host}-{version}{ext}",
            SDK_README_TEMPLATE,
        ))
    if not args.toolchains_only:
        packages.append((
            "游戏存档包",
            lambda: collect_gamesave(repo_root),
            f"tcmips-gamesave-{version}{ext}",
            GAMESAVE_README_TEMPLATE,
        ))

    errors: list[str] = []
    packed = 0
    try:
        for label, collect, out_name, tpl in packages:
            print(f"== 收集{label} ==")
            try:
                items = collect()
                items["README.md"] = write_temp_text(
                    tpl.format(version=version, note=NOTE_TEXT))

                # 工具链包额外生成面向 AI 的 AGENTS.md（包根 + examples 各一份），
                # 让各类 AI agent 不看 README 也能拿到关键硬约束。
                if tpl is SDK_README_TEMPLATE:
                    items["AGENTS.md"] = write_temp_text(
                        SDK_AGENTS_TEMPLATE.format(version=version,
                                                   prefix="",
                                                   readme="README.md"))
                    items["examples/AGENTS.md"] = write_temp_text(
                        SDK_AGENTS_TEMPLATE.format(version=version,
                                                   prefix="../",
                                                   readme="../README.md"))

                out_path = os.path.join(out_dir, out_name)
                make_archive(out_path, out_fmt, items)
                packed += 1
                print(f"  ✓ {label}: {out_path}  ({len(items)} 项)")
            except (FileNotFoundError, RuntimeError) as e:
                errors.append(f"{label}收集失败：{e}")
    finally:
        for t in _TEMP_FILES:
            try:
                os.unlink(t)
            except OSError:
                pass

    if errors:
        print("\n[错误] 以下内容未打包：")
        for e in errors:
            print("  -", e)
        print("\n脚本只收集不编译：请先构建好对应的前置产物，或修正参数后重试。")
        return 1

    if not packed:
        print("\n没有生成任何包（请检查参数）。")
        return 1

    print("\n全部打包完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
