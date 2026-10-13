# TCMIPS Emulator

TCMIPS 架构的桌面模拟器，在宿主上加载并运行编译好的 `.tcm` 镜像，提供像素/text console、
键盘等外设的渲染与交互。支持通过 vtable 在多套 CPU 解释器之间切换（fast / jit / debug）。

## 架构

```
main.c
  memory_init(image, size)         ← 清空 64 MB 并加载镜像
  peripheral_init()                ← 初始化 console / keyboard 互斥锁
  host_run(fps, hold, debug_mode)
    cpu_thread_fn()
      interpreter->execute()       ← g_cpu_fast / g_cpu_jit 或 g_cpu_debug
      interpreter->get_exit_code()
    main_render_loop()             ← MiniFB 窗口、帧节奏、console 叠加渲染
  peripheral_destroy()
```

模块职责：

| 模块 | 职责 |
|------|------|
| `memory.c` | 64 MB guest RAM（`g_emu_memory`），`static inline` `mem_read_`/`mem_write_*`，`memory_init()` |
| `cpu_fast.c` | 快速解释器，computed-goto dispatch，导出 vtable 实例 `g_cpu_fast` |
| `cpu_jit.c` | JIT 解释器，SLJIT 动态编译为 native 代码，block cache + 回填机制 |
| `cpu_debug.c` | 调试解释器，逐指令执行，配合后台 RSP stub 线程驱动暂停/单步/续跑/断点 |
| `peripheral.c` | syscall 分发、console 配置快照、键盘队列、semihost stdin（锁全在模块内部） |
| `host.c` | MiniFB 窗口/渲染、键盘翻译、运行标志、帧计时器 |
| `main.c` | 入口 + 资源生命周期（`memory_init` → `peripheral_init` → `host_run` → `peripheral_destroy`） |

核心原则：

* **Interpreter vtable** — `cpu_interpreter_t` 统一定义 `execute` / `get_count` / `reset_count` /
  `get_exit_code`，host.c 在运行时根据 `-mode=` 选择 fast / jit / debug 解释器，零耦合。
  JIT 模式（`-mode=jit`）不提供 GDB 调试能力。
* **Memory 独立** — `g_emu_memory` 归属于 `memory.c/h`，CPU、外设、渲染回路均通过
  `memory.h` 的 `static inline` 访问，无函数调用开销。
* **Peripheral 封装** — `host_render_frame` 不碰 console 互斥锁或 `TcmConsoleVram` 内部字段，
  只调用 `peripheral_console_get_info()` 拿到一致的配置快照。host 不知道内部锁的名字。
* **统一生命周期** — 所有共享资源的 init/destroy 集中在 `main.c`，子系统不自己管理锁的创建/销毁。

## 构建

```shell
cmake -G Ninja -S emulator -B emulator/build -DCMAKE_BUILD_TYPE=Release
cmake --build emulator/build
```

## 用法

```text
TCMIPSEmulator <image.tcm> [options]
```

| 参数 | 说明 |
|------|------|
| `<image.tcm>` | 要加载并运行的 TCMIPS 镜像 |
| `-stdin=<file>` | 把 semihost stdin 重定向到文件 |
| `-fps=<1-120>` | 目标帧率，默认 60，0 表示不限帧率 |
| `-hold=<0,1>` | 默认 1（退出后保持窗口打开），0 = 自动退出（CI/自动化用） |
| `-mode=<mode>` | 选择解释器：`jit`（默认）、`fast`、`debug`；仅 `debug` 模式支持 GDB 连接 |
| `-mhz=<MHz>` | 把 `fast` 解释器执行速度上限设为约 `MHz` MHz（0/缺省 = 不限速），用于在宿主机上近似真实硬件（如 7 MHz）的执行节奏，让在真实硬件上会卡顿的慢操作在模拟器里也变得可感知。仅 `fast` 模式生效，`jit`/`debug` 忽略 |

窗口标题每秒更新一次：显示运行时长 + 有效指令频率。
Guest 通过 `BREAK(0x3ff)` 退出：`$a0` 表示原因（0 正常退出，非 0 为 abort），
`$a1` 作为进程退出码。

## 输出与验证

* guest 的 `printf` / `write` 输出被转送到模拟器的 **stdout**，可被脚本捕获。
* guest 通过 `BREAK(0x3ff)` 的退出码作为进程退出码。
* guest 正常退出后，模拟器还会向 **stderr** 追加一行 guest 累计执行的指令总数：
  `!! TCMIPS TOTAL INSNS = <N> !!`。该行独立于 `EXIT` 行，仅在 guest 正常执行
  完成（`execute()` 返回）后输出，用于性能对比（如软浮点 vs 定点）。不要用
  `<N>` 参与退出码判定——退出码仍以进程退出码为准。
* 这三个能力构成 semihost 自动化测试的基础，详见 [`AUTOTEST.md`](../AUTOTEST.md)。

### 退出码约定

模拟器进程的退出码被划分为两个互不重叠的区间：

- **guest 退出码 `[0, 127]`**：guest 通过 `BREAK(0x3ff)`（`$a1` = 退出码）正常/失败
  结束。guest 侧的 `_exit()` 会把退出码 `& 0x7F`，因此 guest 无论传什么值，最终都落在
  `[0, 127]`，**不可能**与模拟器自身错误码（≥128）冲突。
- **模拟器错误码 `[128, 255]`**（定义于 `emulator_exit.h`）：

| 退出码 | 宏 | 含义 |
|--------|-----|------|
| `130` | `TCM_EMULATOR_EXIT_WINDOW_CLOSED` | 用户手动关闭窗口（128 + SIGINT） |
| `131` | `TCM_EMULATOR_EXIT_MEMORY_FAULT` | 非法内存访问（越界 / guard page） |
| `132` | `TCM_EMULATOR_EXIT_USAGE` | 未提供镜像参数（用法错误） |
| `133` | `TCM_EMULATOR_EXIT_BAD_STDIN` | `-stdin=<file>` 设置失败 |
| `134` | `TCM_EMULATOR_EXIT_LOAD_FAILED` | `.tcm` 镜像加载失败 |
| `135` | `TCM_EMULATOR_EXIT_INTERNAL` | 内部错误（未知 `-mode=` / 线程创建 / JIT 失败） |

关键区分点：

- `130`：stderr 输出 `!! TCMIPS EMULATOR STOPPED BY USER !!`（`STOPPED BY USER` 而非
  `EXIT`），`exit(130)` 承载，区别于 guest 退出，不要误判为测试失败。
- `131`：stderr 输出 `!! TCMIPS EMULATOR MEMORY FAULT !! ...`，表示 guest 发生了越界。

### 内存访问校验

`memory.h` 的 `mem_read_*` / `mem_write_*` 访问器会对每次访存做合法性校验：任一字
节越出 64 MB（`[0, 0x04000000)`），或读写落进 4 KB **guard page**
（`0x03BBF000` ~ `0x03BC0000`，堆/栈之间的溢出哨兵），即判定为非法访问——stderr 输出
`!! TCMIPS EMULATOR MEMORY FAULT !! read/write addr=0x........ width=N`，进程以
**131** 退出。

> **注意**：该校验只在经过 `memory.h` 访问器的数据路径上生效——`fast` / `debug` 解释器
> 及 host 侧 helper 都走这里，**JIT 模式的数据访存由 SLJIT 生成的原生代码直接读写，不
> 经过访问器**，因此不触发该校验。自动化测试（希望覆盖非法访存检测时）应使用
> `-mode=fast`，见 `AUTOTEST.md`。

## 运行示例

```text
# 自动化自检测试（无交互，以退出码反映结果）
./TCMIPSEmulator test_sdl1_autotest.tcm -hold=0

# 交互式演示，限制 30 帧/秒
./TCMIPSEmulator demo.tcm -fps=30

# 使用调试解释器 + GDB RSP stub（默认监听 TCP :1234，需 -mode=debug）
./TCMIPSEmulator demo.tcm -mode=debug

# 使用 JIT 解释器（默认模式，无 GDB 调试能力）
./TCMIPSEmulator demo.tcm -mode=jit
```

## 调试器支持

`-mode=debug` 参数启动调试解释器（`cpu_debug.c`）并拉起一个后台 GDB RSP stub 线程
（`rsp_stub.c`，默认监听 TCP :1234），实现以下调试能力：

> 注意：GDB 调试仅在 `-mode=debug` 模式下可用。`-mode=jit` 和 `-mode=fast` 不支持 GDB 连接。

| 能力 | RSP 协议 | 说明 |
|------|----------|------|
| 寄存器读写 | `g`, `G`, `p`, `P` | 读取/修改全部 32 个 GPR |
| 内存读写 | `m`, `M` | 读写 guest 地址空间 |
| 软件断点 | `Z0`, `z0` | 地址表建模，不改写镜像 |
| 执行控制 | `c`, `s`, `vCont` | 续跑、单步、步过 (`vCont;c/s/C/S`) |
| 中断 | `^C` (0x03) | RSP 线程 select 轮询，运行期间可随时中断 |
| 进程退出应答 | `W` | 停止原因 + 退出码上报 |

源码级调试通道：

- Clang `-g -gdwarf-4` 把 DWARF 调试信息嵌入 LLVM IR（`.bc` 文件携带 `!dbg` 元数据）。
- Debug 构建时链接器 `llvm-link-tcmips -tcm-debug=1` 把 DWARF 写到最终镜像。
- GDB / LLDB 以 `mipsel-remote` 目标连接 TCP :1234，即可断点、单步、查看调用栈和源码行号。