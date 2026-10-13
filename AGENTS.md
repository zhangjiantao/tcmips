# AGENTS.md — AI 助手协作约定

本文件为 AI 助手在本仓库工作时必须遵循的工程约定。
目标是**减少盲目探索、直接复用既有事实**：凡能提前确定的工具链、路径、命令、判定标准，
都固化在此，AI 应优先照做，而非从头试探。

---

## 1. 项目概述

TCMIPS 全生态：TCMIPS CPU 架构 + `tcmips_core` 库 + 移植的 picolibc / libc++ / compiler-rt，
以及一套面向 `Turing Complete` 沙盘模拟器的完整 C/C++ 交叉编译工具链
（改造过的 Clang + 自定义链接器 `llvm-link-tcmips`），编译产物为 `.tcm` 可执行文件。

关键目录：

| 路径 | 说明 |
|---|---|
| `toolchains/` | 交叉工具链（llvm/clang、cmake toolchain 文件、sysroot） |
| `emulator/` | 宿主机 TCMIPS 模拟器源码 |
| `test/` | 各类测试（memfs / picolibc / sdl1 / sdl2 / cg / stb / misc / cxx） |
| `tcmips/` | 运行时与标准库移植（bsp/ 启动+POSIX 桥接、dev/ 外设、lib/ 标准库与兼容层） |
| `examples/` | 最小端用户示例工程（`hello` 程序：printf "Hello, world!" 交叉编译为 `.tcm`，随工具链包分发，开箱即编译） |
| `cmake-build-release/` `cmake-build-debug/` | 用户自己的构建目录。**仅从 `CMakeCache.txt` 提取工具链信息**（见 §2.2）；**例外：打包须在此目录触发 `PACK_RELEASE` 目标**（见 §7) |

> 移植第三方库 / 兼容层（在 `tcmips/lib/` 下新增 `<libxxx>`）时，遵循
> [`docs/TCMIPS-Porting.md`](docs/TCMIPS-Porting.md) 的统一范式：vendor 溯源注释、
> CMakeLists 三件事（add_library + 头同步 + 按需 link）、实现映射到 tcmips_dev/memfs、
> 接入 AUTOTEST。

---

### 1.1 工作前置：先确定系统环境与 shell

**在做任何具体工作（编译、运行、探测）之前，先确定当前系统环境**：

- **Windows 环境**：优先使用 **Python 脚本**（若系统已装 Python，用它写/执行探测、
  文件操作等任务，语法确定、跨平台且无 shell 差异）；其次用 **PowerShell**（调用
  `powershell`）；**避免使用 `cmd.exe`**（`cmd` 的 `echo` 中文、`for`/`where` 语法、
  `printf` 缺失等问题会导致命令被误解析或静默失败）。若确需用 `cmd`，注意其语法差异。
- **非 Windows 环境（macOS / Linux）**：自行确定可用的 shell（如 `bash` / `sh` /
  `zsh`），用 `which` / `command -v` 探测，无需特殊指定。

确定系统与 shell 后再继续下面的工具链定位、构建与运行。

---

### 1.2 交流约定

- 面向用户的所有自然语言（总结、计划、说明、思考）一律使用**简体中文**。
- 代码、命令、路径、日志、提交信息原文保持原语言，不翻译。
- 结论要基于事实与实测，客观中立；不做过度赞美或迎合。

---

### 1.3 Git 与工作区卫生

- **不执行任何 git 变更操作**：不 commit、不 push、不 merge、不打 tag、不切分支，除非用户明确要求。
- **临时性输出一律写入系统 tmp 目录，不要散落在仓库工作区**：
  - 系统 tmp 目录的指代：Windows 用 `%TEMP%`（通常 `C:\Users\<user>\AppData\Local\Temp`）；
    POSIX 用 `$TMPDIR`（未设置时回退 `/tmp`）。
  - 系统 tmp 目录不可用时，可在项目根目录下创建 temp/ 目录使用。
  - 只有「本身就是项目资产」的文件（源码、测试、文档、构建产物）才写在仓库内。

---

## 2. 编译环境定位（重要：不要盲目 where/ls）

### 2.0 本工程有「两套编译环境」，先分清再动手

本仓库编译**涉及两个完全独立的编译环境**，编译对象、编译器、链接器、sysroot 都不同，
**切勿混用**。开始任何编译前先把二者对号入座：

| 维度 | 交叉编译环境（guest 侧） | host 编译环境（模拟器侧） |
|---|---|---|
| 编译对象 | TCMIPS guest 程序（标准库 / 运行时 / 测试 / demo） | TCMIPS 模拟器本体（普通宿主机 C 工程） |
| 产物 | `.tcm` 可执行文件（跑在 TCMIPS CPU 上） | 宿主机可执行文件（如 `TCMIPSEmulator.exe`） |
| 编译器 | `toolchains/llvm/bin/clang` / `clang++`（改造过的 Clang） | 宿主机自身编译器：**MinGW 或 MSVC**（见 §2.4） |
| 链接器 | `toolchains/llvm/bin/llvm-link-tcmips`（自定义） | 宿主机原生 `ld` / `link` |
| toolchain cmake | `toolchains/cmake/tcmips.toolchain.cmake`（**必须**，见 §2.3） | 无需 toolchain 文件 |
| sysroot | `toolchains/sysroot`（picolibc / libc++ / compiler-rt） | 宿主系统头文件与库 |
| 目标 triple | `mipsel-unknown-elf`（mips32r2, soft-float, abi=32） | 宿主架构（如 `x86_64-w64-mingw32` / msvc） |
| 构建目录（AI 独立） | `cmake-build-autotest` | `cmake-build-autotest-emulator` |
| 完整命令 | 见 §3.2 / §6.2 | 见 §3.1 / §2.4 |

后续小节归属如下：**§2.1 / §2.2 / §2.3 属于交叉编译（guest）侧**，**§2.4 属于 host（模拟器）侧**。

### 2.1 构建系统

- 构建系统：**CMake + Ninja**（两套环境下生成器都固定为 Ninja，见 §3.3）。
- 交叉编译目标：`mipsel-unknown-elf`（mips32r2, soft-float, abi=32）。

### 2.2 cmake 的定位规则（交叉侧）

**禁止**直接 `where cmake`、`which cmake` 或扫描整个磁盘去找 cmake。

正确做法（按优先级）：

1. 从用户既有的 `cmake-build-release/` 或 `cmake-build-debug/` 的 `CMakeCache.txt` 中
   **仅提取工具链信息**（这些目录是用户自己的构建目录，只读、不在其中编译）：
   - `CMAKE_COMMAND:INTERNAL=...` → cmake 可执行文件的绝对路径；
   - `CMAKE_GENERATOR:INTERNAL=` → 确认是 Ninja；
   - `CMAKE_TOOLCHAIN_FILE:FILEPATH=...` → 交叉工具链 cmake 文件路径。
2. 拿到绝对路径的 cmake 后，**在 AI 自己的独立构建目录**中编译，**不要写用户已有的
   `cmake-build-release/debug`**。构建目录的命名与完整命令见 §3（`cmake-build-autotest`
   与 `cmake-build-autotest-emulator`），此处不重复。
3. 若独立构建目录尚未生成，§3 里的 `-S ... -B ...` 命令本身就是新建命令（含工具链
   文件），无需额外步骤。

### 2.3 交叉工具链的定位规则（交叉侧）

检查 `toolchains/llvm/bin/` 下是否存在以下可执行文件（**文件名不假定后缀**：
Windows 上为 `.exe`，macOS/Linux 上通常无后缀，按实际存在形式判断）：
- `clang`（C 编译器）
- `clang++`（C++ 编译器）
- `llvm-link-tcmips`（自定义链接器）

- **存在** → 直接使用（工具链 cmake 文件里也是引用这些路径）。
- **不存在** → **停止编译相关任务**，明确告知用户：
  "前置工具链缺失（toolchains/llvm/bin/ 下无 clang / clang++ / llvm-link-tcmips），
  无法继续编译，请先补齐工具链。"

不要绕过工具链做徒劳的替代尝试。

### 2.4 host 编译环境（构建模拟器用，host 侧）

模拟器是**宿主机（host）侧**的普通 C 工程，**不做交叉编译**，用宿主机自身的 C/C++
编译器，与 guest 侧的 `toolchains/llvm` 交叉工具链是两套，切勿混用（见 §2.0 对照表）。

**host 侧编译器不唯一**：`emulator/CMakeLists.txt` 兼容 **MinGW（GCC 系）** 与
**MSVC** 两种宿主机编译器（`if (WIN32)` / `if (MINGW)` 分别处理），究竟用哪一种，取决于
用户当初是用哪个生成器/编译器配置过该工程。因此**不要默认 host = MinGW**，而是以
下述定位结果为准。

**定位规则（关键，避免在错误的生成器/编译器上来回试探）：**

1. **Windows 环境**：优先探测 `emulator/cmake-build-release/CMakeCache.txt`（或
   `emulator/cmake-build-debug/CMakeCache.txt`）：
   - 若其中已有用户实际使用过的 host 工具链（`CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER`
     / `CMAKE_MAKE_PROGRAM` / `CMAKE_GENERATOR`），**直接复用**这些值；
   - 若既无该文件、也没有可复用的工具链信息，**停止工作，向用户询问** host 编译
     环境如何搭建，不要擅自猜测或从零摸索。
     - **MinGW 分支**：`CMAKE_C_COMPILER` 通常是 `.../mingw64/bin/cc.exe`（C）与
       `.../c++.exe`（C++）。此时**构建前把编译器所在目录加入 `PATH`**：MinGW 的
       `cc.exe` 运行时依赖同目录下的 DLL（如 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`）
       与子工具（`as` / `ld` / `collect2` 等），若该 bin 目录不在 `PATH`，cmake 的
       「working C compiler」探测会反复失败。可用 Python 脚本把该目录注入当前进程的
       `PATH` 后再执行构建（优先，见 §1.1）；或 PowerShell 里
       `$env:PATH = "<编译器bin目录>;" + $env:PATH`。
     - **MSVC 分支**：`CMAKE_C_COMPILER` 形如 `.../cl.exe`。此时**不要**套用上面的
       MinGW 的 `PATH` 注入说法；MSVC 需要的是在「Visual Studio 开发者命令提示符」环境
       下（`vcvars*.bat` 已生效）调用 cmake。若缓存显示用户用的是 MSVC 但当前 shell 不是
       VS 环境，按需提示用户或复用其已配置好的构建命令，不要擅自用 MinGW 替代。
2. **非 Windows 环境**：自行寻找主机编译器（如 `gcc` / `cc` / `clang`，通常已在
   `PATH` 中），无需特殊指定。

---

## 3. 构建与运行测试

**以 `AUTOTEST.md` 为权威指导**。标准流程（构建目录为 AI 独立目录，见 2.2）：

1. 构建模拟器（host 侧，非交叉，见 §2.4；**AI 自行构建模拟器，不复用用户已有的
   `emulator/cmake-build-release/TCMIPSEmulator.exe`**）：
   ```text
   "<CMAKE_COMMAND>" -S emulator -B cmake-build-autotest-emulator -G Ninja \
     -DCMAKE_MAKE_PROGRAM="<host ninja>" \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_C_COMPILER="<host cc>" -DCMAKE_CXX_COMPILER="<host c++>"
   "<CMAKE_COMMAND>" --build cmake-build-autotest-emulator --target TCMIPSEmulator
   ```
2. 构建被测 guest（交叉编译）：
   ```text
   "<CMAKE_COMMAND>" -S . -B cmake-build-autotest \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE=toolchains/cmake/tcmips.toolchain.cmake
   "<CMAKE_COMMAND>" --build cmake-build-autotest --target <test_target>
   ```
3. 用模拟器运行（无交互，自动退出；**必须 `-mode=fast`** 以启用访存校验，见 §3.2）：
   ```text
   cmake-build-autotest-emulator/TCMIPSEmulator cmake-build-autotest/autotest/<target>.tcm -hold=0 -mode=fast
   ```

   运行命令中的 `TCMIPSEmulator` 按实际产物文件名书写：Windows 上为 `TCMIPSEmulator.exe`，
   其余平台通常无后缀。判断标准见 §3.2。

### 3.1 构建目录命名

参考 AUTOTEST.md 中使用的 `cmake-build-autotest(-emulator)` 目录名（供人/机器阅读与
直接执行）。AI 执行时**遵守此约定**，避免污染用户已有的
`cmake-build-release/debug`：

- 模拟器（host）：`cmake-build-autotest-emulator`
- 被测 guest（交叉）：`cmake-build-autotest`
- 产物位置：`cmake-build-autotest/autotest/<target>.tcm`

其余命令参数（工具链文件、`-DCMAKE_BUILD_TYPE`、目标名等）与 AUTOTEST.md 完全一致，
目录名也沿用 `cmake-build-autotest(-emulator)`（见 §3.1），同时显式补上 `-G Ninja` 等参数（见 §3.3）。

重复执行时可直接 `--build` 已存在的上述目录，无需每次都重新 `-S ... -B ...`。

### 3.2 交互、运行模式与退出码

- 运行 guest 测试一律使用 `-hold=0`（hold参数为0表示自动退出，不等待交互输入）。
- **运行模式必须使用 `-mode=fast`**（不要用默认的 `-mode=jit`）：JIT 模式的数据访存由
  SLJIT 生成的原生代码直接读写，不经过 `memory.h` 访问器，因此**不触发非法访存校验**；
  只有 `fast`（或 `debug`）模式才走 `mem_read_*`/`mem_write_*`，能拦截越界并确定性报错。
- 判定标准以 AUTOTEST.md 为准：
  - guest 通过 `printf` 输出 `[SELF] <name> PASS/FAIL`、`[RESULT] pass=N fail=M`；
  - 通过 exit syscall 输出退出码到 stderr：`!! TCMIPS EMULATOR EXIT, code=N !!`；
  - **退出码 0 + 无 FAIL = 全部通过**；退出码非 0 或存在 `[SELF] ... FAIL` = 存在失败。
- 模拟器进程退出码划分为两个互不重叠的区间，判定时须区分：
  - **guest 退出码 `[0, 127]`**：guest 通过 `BREAK(0x3ff)`（`$a1`）结束；guest 侧
    `_exit()` 会 `& 0x7F`，因此 guest 到不了 ≥128 的值。`0 + 无 FAIL` = 通过，非 0 = 失败。
  - **模拟器错误码 `[128, 255]`**（定义于 `emulator/emulator_exit.h`）：
    - `130` 关窗（`STOPPED BY USER`，非 guest 失败）；
    - `131` 非法访存（`MEMORY FAULT`，guest 越界 bug，**应视为失败**）；
    - `132` 用法错误 / `133` stdin 失败 / `134` 加载失败 / `135` 内部错误（均为环境问题）。
  - 判定以「退出码区间 + stderr 文案」为准：`STOPPED BY USER`(130) 与 `MEMORY FAULT`(131)
    分别区别于 guest 的正常 `EXIT, code=N`。

### 3.3 Windows 上的生成器注意点

在 Windows 上，`cmake` 默认可能选择 **NMake**（而 nmake 常缺失，会直接配置失败）。
因此**无论 host 侧还是 guest 侧**，配置时都要**显式 `-G Ninja`**；host 侧还要
`-DCMAKE_MAKE_PROGRAM=<host ninja>`（见 §2.4）。guest 侧的交叉编译命令应写为：

```text
"<CMAKE_COMMAND>" -S . -B cmake-build-autotest -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/cmake/tcmips.toolchain.cmake
```

（§6.2 回归测试中的命令示例同理，已等效为带上 `-G Ninja`。）

---

## 4. 代码风格与质量标准

### 4.1 格式化

- **强制使用根目录 `.clang-format`**（文件已存在于仓库根目录）。
- 新写或修改的 C/C++ 代码必须符合该风格（本仓库 test 代码实际采用 2 空格缩进、
  K&R 大括号风格，参考 `test/memfs/test_memfs_autotest.c`）。
- 格式化工具（clang-format 可执行文件）不强求某特定位置：若 `toolchains/llvm/bin/`
  或系统 PATH 中有可用的 `clang-format` 则使用；没有则手动遵循同一风格，并提醒用户。

### 4.2 编译质量

- 必须做到**零警告、零错误**后方可认为编译通过。
- 工具链已默认开启 `-Werror=implicit-function-declaration -Werror=implicit-int
  -Werror=return-type` 等（见 `toolchains/cmake/tcmips.toolchain.cmake`），
  implicit 声明会被视为 error，务必先 include 正确头文件再调用接口。

### 4.3 可移植性

- 引用平台相关常量/接口前，先确认 picolibc（sysroot 头文件）是否提供；
  不确定的接口宁可加注释说明，也不要臆造不存在的符号。
  例如 `open_memstream` 在 picolibc 中不存在（只有 `fmemopen`）；`memalign`
  需额外 `#include <malloc.h>` 才声明。
- **picolibc 与 glibc/musl 存在语义差异**，不要照搬标准库的预期行为：
  - `getdelim`/`getline` 在读到 EOF 且无新字符时**返回 0**，而非标准 POSIX 的 `-1`
    （见 `tcmips/lib/picolibc-1.8.11/libc/stdio/getdelim.c`）。
  写断言前先查该接口在 picolibc 中的实际实现，而不是凭标准库经验猜。
- 实测运行是最终验证标准："编译通过" 不等于 "断言通过"，能跑模拟器时务必实机验证。

### 4.4 TCMIPS 运行时约束（C++ / 交叉工具链）

- **libc++ 以 `-fno-exceptions` 交叉**：任何会抛异常的操作（含 try/catch 可达路径）
  会转而调用 `abort()` 使 guest 崩溃。因此**不要依赖异常**；可能失败的操作一律走
  `error_code` 重载、返回值/`errno` 判断，而非 throw（参见 `test/memfs/test_memfs_autotest.cpp`
  顶部注释）。
- **C++ 标准版本**：工具链的 libc++ 14 对部分 C++20 特性支持有限，聚合/新写 C++
  测试时**优先 `-std=c++17`**。
- 交叉编译时 cmake 输出大量 `-- Looking for ... not found` / `Performing Test ...
  Failed` 属正常探测噪音，**看最终的 `Configuring done` / `Build files have been
  written` 判定配置是否成功**，不要误判为失败。

---

## 5. 常见任务速查

### 5.1 编译某个测试目标（以 picolibc 为例）

```text
# 1) 定位 cmake（读 CMakeCache.txt，见 2.2）
# 2) 构建单个目标（AI 独立目录 cmake-build-autotest）
"<CMAKE_COMMAND>" --build cmake-build-autotest --target test_picolibc_autotest
```

### 5.2 运行该测试

```text
# 先构建模拟器（见 3），产物路径示例：
cmake-build-autotest-emulator/TCMIPSEmulator \
  cmake-build-autotest/autotest/test_picolibc_autotest.tcm -hold=0 -mode=fast
```

判定：stdout 出现 `[RESULT] pass=N fail=0`，
stderr 出现 `!! TCMIPS EMULATOR EXIT, code=0 !!`。
（必须 `-mode=fast`；参见 §3.2 关于访存校验与退出码 130/131 的说明。）

### 5.3 遇到"工具链缺失"时

无须自行排查，明确停止，并提醒用户补齐，见 2.3。

---

## 6. 回归测试

当任务涉及「改动可能影响运行时 / 标准库 / 外设 / 兼容层」时，应执行一次完整的回归测试。

### 6.1 定义

回归测试 = **构建 `AUTOTEST_ALL` 目标，然后用模拟器逐个运行构建目录
`autotest/` 路径下的所有 `.tcm` 产物，目标列表以 `test/CMakeLists.txt` 为准，须全部 PASS**（每个都以退出码 `0` 结束，
且无 `[SELF] ... FAIL` 行）。

`AUTOTEST_ALL` 是一个聚合目标（定义于 `test/CMakeLists.txt`），其依赖的全部
autotest 目标产物统一输出到 `${CMAKE_BINARY_DIR}/autotest/`（即
`cmake-build-autotest/autotest/`）。`${CMAKE_BINARY_DIR}/autotest/` 目录外的其他目标产物不进行测试。

### 6.2 执行步骤

1. 构建模拟器（若尚未构建，见 §3）。
2. 一次性构建全部 autotest 目标：
   ```text
   "<CMAKE_COMMAND>" -S . -B cmake-build-autotest -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE=toolchains/cmake/tcmips.toolchain.cmake
   "<CMAKE_COMMAND>" --build cmake-build-autotest --target AUTOTEST_ALL
   ```
3. 枚举 `cmake-build-autotest/autotest/` 下的所有 `.tcm` 文件，逐个用模拟器运行：
   ```text
   cmake-build-autotest-emulator/TCMIPSEmulator \
     cmake-build-autotest/autotest/<每个>.tcm -hold=0 -mode=fast
   ```

### 6.3 判定标准

判定标准以 AUTOTEST.md 为准，对每个 `.tcm` 逐一判定：

- **全部通过**：每一个 `.tcm` 满足：
  - 退出码 `0`，stderr 出现 `!! TCMIPS EMULATOR EXIT, code=0 !!`；
  - stdout 输出 `[SELF] <name> PASS` 断言，且出现 `[RESULT] pass=N fail=0`。
- **任一失败**：某个 `.tcm` 退出码非 0，或输出中出现
  `[SELF] ... FAIL` / `FAIL` / `SOME TESTS FAILED`，即视为回归失败。
- **退出码辨析**（不可一律按「非 0 = 失败」处理）：
  - `[0,127]` 是 guest 退出码（`_exit` 已 `& 0x7F`），`0` = 通过，`1~127` = guest 失败；
  - `130`（`STOPPED BY USER`）= 用户手动关窗，非 guest 失败，不计入回归失败；
  - `131`（`MEMORY FAULT`）= guest 非法访存（越界 / guard page），**计入回归失败**；
  - `132~135`（用法/stdin/加载/内部错误）= 环境问题，非 guest 失败，应排查运行环境。

### 6.4 失败处理

- 回归失败时，定位出是哪个 `.tcm`、哪个 `[SELF]` 用例失败，结合该用例的
  `FAIL (detail)` 定位原因，修复后**重新构建并重跑该目标**，直至全部 PASS。
- 不得在仍有 FAIL 的情况下宣称「回归通过」。

## 7. 打包

仅用户要求时触发。

**前置质量校验**：确认回归测试全部 PASS（判定标准见 §6.3）。

**打包执行方式**：**在用户构建目录** `cmake-build-release` 中触发 `PACK_RELEASE`：

```text
"<CMAKE_COMMAND>" --build cmake-build-release --target PACK_RELEASE
```

原因：demo `.tcm` 由 `demo/CMakeLists.txt` 输出到构建目录 `cmake-build-release/demo/`，
`pack_release.py` 再从该目录二次收集 `.tcm`（同时收集模拟器可执行文件），统一放进工具链包内
与 `toolchains/` 平级的 `emulator/` 目录（`emulator/TCMIPSEmulator` + `emulator/demo/*.tcm`）。
这些构建产物依赖 `DEPENDS` 在同一构建目录下被先行产出，因此在其他目录触发会因缺少前置产物而报错。
打包失败立即中止并报告用户，无需自行排查。
打包成功后，产物位于 `dist/` 目录：
- `dist/tcmips-toolchains-<host>-<version>.tar.gz` — 工具链包（含模拟器与 demo）
- `dist/tcmips-gamesave-<version>.tar.gz` — 游戏存档包
将结果通知用户。
