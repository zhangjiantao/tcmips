# TCMIPS 模拟器自动化测试方法（基于 semihost 机制）

本文档介绍一种通用的 TCMIPS guest 程序自动化测试方法论：
利用模拟器的 semihost（轻量主机 I/O）机制，让运行在 TCMIPS 上的 **任何 guest 程序**
通过 `printf` 报告测试结果、通过退出码表达运行结论，从而无需人工交互、无需调试器，
即可自动验证目标代码的正确性与边界稳定性。

这套方法与被测对象无关，适用于标准库 / 运行时、外设驱动、兼容层、移植游戏等各类 guest 工程。

## 1. 机制：semihost 的 stdout 与退出码

TCMIPS 模拟器为 guest 提供两个关键能力，它们是整套方法的基础：

1. **guest 的 `printf` / `write` 输出被转发到模拟器的宿主 stdout。**
   guest 无需真实终端即可把日志呈现给宿主脚本。

2. **guest 通过 syscall 发出的退出码被模拟器写入 stderr，格式为 `!! TCMIPS EMULATOR EXIT, code=N !!`。**
   宿主脚本可以用退出码作为自动化测试的通过 / 失败判据。
   由于走 stderr，可与 guest 的 stdout 内容天然分离（shell 用 `2>` 单独捕获）。

由此，任意 guest 程序都可以做到：

- 用普通 `printf` 打印结构化结果。
- 结尾打印汇总信息。
- 用退出码（0=通过、非 0=失败）传递整体结论。
- **不依赖任何交互输入，运行完自动退出**，适合 CI / 脚本自动化。

## 2. 测试框架通用设计

一套可复用的框架应包含以下部分：

### 2.1 结构化断言宏

所有断言输出统一为 `[SELF] <名称> PASS/FAIL` 单行，便于 grep / 脚本统计：

```c
#define SELF_CHECK(name, cond) {        /* PASS/FAIL 计数 + 输出单行 */ }
#define SELF_EQ_X32(name, a, e) {       /* 数值相等断言 + 输出差异 */ }
```

要点：

- 每个用例一条独立的、可识别名称的行。
- FAIL 时尽量附带「实际值 vs 期望值」，便于快速定位。
- 用静态计数器累计 `pass` / `fail`。

### 2.2 汇总与退出码约定

结尾统一输出汇总并返回退出码：

```c
printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
return (g_fail == 0) ? 0 : 1;   /* 0 = 全部通过，非0 = 存在失败 */
```

- 宿主从模拟器 stdout 捕获 `[RESULT]`，或从 stderr 读 `!! TCMIPS EMULATOR EXIT, code=N !!`。
- 两者可互为校验。

### 2.3 退出码约定：区分「guest 失败」与「模拟器自身异常」

模拟器进程的退出码划分为两个互不重叠的区间，自动化判定不能只看退出码是否非 0：

- **guest 退出码 `[0, 127]`**：guest 通过 `BREAK(0x3ff)`（`$a1`）结束。guest 侧的
  `_exit()` 会把退出码 `& 0x7F`，因此 guest 无论如何都到不了 ≥ 128 的值。
- **模拟器错误码 `[128, 255]`**（定义于 `emulator/emulator_exit.h`）：

| 退出码 | 含义 | 判定 |
|--------|------|------|
| `130` | **用户手动关闭窗口**（128 + SIGINT） | 非 guest 失败，不要误判为回归失败 |
| `131` | **非法内存访问**（越界 64 MB 或读写 guard page） | guest 有越界 bug，视为失败 |
| `132` | 未提供镜像参数（用法错误） | 环境/调用问题，非 guest 失败 |
| `133` | `-stdin=` 设置失败 | 环境问题 |
| `134` | `.tcm` 镜像加载失败 | 环境问题 |
| `135` | 内部错误（未知 `-mode=` 等） | 环境问题 |

区分要点：

- `130`：stderr 输出 `!! TCMIPS EMULATOR STOPPED BY USER !!`（`STOPPED BY USER` 而非
  `EXIT`），进程以 `exit(130)` 承载。
- `131`：stderr 输出 `!! TCMIPS EMULATOR MEMORY FAULT !! read/write addr=0x........ width=N`，
  进程以 `exit(131)` 承载；表示 guest 发生了堆栈越界（读写 guard page）或地址越界。

### 2.4 运行模式与访存校验

模拟器的内存访问校验（`131` 那个判定）**只在经过 `memory.h` 访问器的数据路径上生效**：

- `fast` / `debug` 解释器及 host 侧 helper 都走 `mem_read_*` / `mem_write_*`，**会校验**；
- `jit` 模式的数据访存由 SLJIT 生成的原生代码直接读写，**不经过访问器**，**不校验**。

因此，**自动化测试必须用 `-mode=fast`**（不要用默认的 `-mode=jit`），才能让非法访存被
拦下并产生确定性的 `131`，而不是在 jit 下退化为未定义/崩溃。运行命令：

```text
cmake-build-autotest-emulator/TCMIPSEmulator \
  cmake-build-autotest/autotest/<target>.tcm -hold=0 -mode=fast
```

### 2.5 有状态接口的测试注意事项

- 若被测对象是**单实例 / 需重建**的资源（如同时只能存在一个的句柄 / 帧缓冲），
  测试要按状态**逐段切换**，避免持有多份已失效的句柄。
- 对会**重建底层资源**的切换操作（如改位深、改分辨率），应“切一次、测一组、再切”。
- 对“硬件强制修正”的行为（如分辨率被立即取整 / 按步进调整），断言应针对**实际返回值**，
  而不是请求值。

### 2.6 冒烟运行

自检可附带一小段真实负载，让被测外设 / 子系统实际被驱动，以暴露运行时错误。
这段负载必须**有界**（有限迭代 / 显式退出条件），否则会拖住自动化。

### 2.7 模糊 / 边界组合

- 以「非法参数、NULL 指针、越界矩形、极端尺寸」作为常规边界输入。
- 需要时可枚举大量随机尺寸与坐标组合做模糊测试，但要控制规模避免超时。

## 3. 构建与运行

```text
# 1) 构建模拟器
cmake -S ./emulator -B cmake-build-autotest-emulator \
  -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-autotest-emulator --target TCMIPSEmulator

# 2) 构建被测 guest 及其依赖
cmake -S . -B cmake-build-autotest \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/cmake/tcmips.toolchain.cmake
cmake --build cmake-build-autotest --target <test_target>

# 3) 用模拟器运行（无交互，自动退出；必须用 -mode=fast 以启用访存校验）
cmake-build-autotest-emulator/TCMIPSEmulator \
  cmake-build-autotest/autotest/<target>.tcm -hold=0 -mode=fast
```

判定标准：

- 退出 `0`（并输出 `PASS`）表示全部用例通过。
- 退出非 0（并输出 `FAIL`）表示存在失败断言。
- 退出 `130`（`STOPPED BY USER`）是用户关窗，非 guest 失败。
- 退出 `131`（`MEMORY FAULT`）表示 guest 非法访存（越界 / guard page）。
- CI / 脚本直接以退出码为评判依据，同时保存 stdout 中的 `[SELF]` 行作为详细报告。

## 4. 方法收益

- **自动发现真实缺陷**：上电即跑，可快速暴露对边界 / 语义的错误处理。
- **回归保护**：被测模块改动后重跑一遍即可做回归。
- **可迁移**：同一套断言与退出码框架可复用到任意 guest 模块，无需平台特定设施。

## 5. 扩展方向

- 对缓冲 / 矩形类操作引入随机模糊测试。
- 对队列类操作测试满 / 空边界。
- 对耗时用例引入「有限帧数 + 帧内超时」防死循环。
- 把 `[SELF]` 输出行接入宿主测试报告 / CI 徽章。