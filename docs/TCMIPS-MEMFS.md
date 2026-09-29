# libmemfs — TCMIPS 内存文件系统

## 1. 是什么

TCMIPS 平台没有磁盘、没有 MMU、没有只读内存：整个 `.tcm` 镜像就是统一 RAM，每个字节都可写。
`libmemfs` 提供一个 POSIX 风格的 BSP 文件层（`open/read/write/lseek/close/stat`），由运行在内存中的
文件系统支持，其文件内容由链接器（`llvm-link-tcmips --tcm-memfs=<dir>`）烧入镜像。

对标 Emscripten 的 MEMFS：文件活在内存里、运行时按路径访问、可写。

## 2. 内存模型与读写语义

```
.tcm 镜像 (统一 RAM，全部可写)
├─ MemfsPayloadHeader         magic + count + data_off
├─ MemfsEntry[count]          每文件一条目录项
├─ per-file segment0          文件的打包内容：固定 RAM，可原地读写
└─ (运行时) Chunk 链          malloc 的固定块，越过 segment0 末尾时追加

open("r")   → 映射打包的 segment0 RAM（未打包名称且非 O_CREAT → ENOENT）
open("w")   → O_WRONLY|O_CREAT|O_TRUNC；打开文件并用 O_TRUNC 清空逻辑大小
open("r+"/"w+") → O_RDWR；可原地改写 segment0，也可越过末尾追加 Chunk
open(O_CREAT) → 目录里不存在的名称需带 O_CREAT 才会新建（同真实 FS）；
             `open(path, O_CREAT, mode)` 会把 mode 保存到新建文件的 st_mode
openat_dir  → 通过相对目录 fd 打开子目录（O_DIRECTORY），为 `remove_all` 提供子目录 fd
unlinkat    → 支持 `unlinkat(dirfd, name, 0)` 与 `unlinkat(dirfd, name, AT_REMOVEDIR)`
dirfd / fdopendir → 为 opendir 得到的目录流分配 synthetic fd
write       → 尊重 O_APPEND；segment0 内原地写 RAM；越过末尾挂 Chunk 链
read/write  → 按打开模式（O_ACCMODE）检查权限，只读 fd 拒绝写、只写 fd 拒绝读
close       → 递减节点引用；若该节点已被删除且引用归零，则释放追加的 Chunk 链；
              segment0（打包 RAM）与未被删除的共享内容继续保留
stat/fstat  → 填写 st_size / st_mode / st_nlink / st_dev / st_ino；
               st_dev 恒为 1，st_ino 取节点指针（libc++ fs::equivalent 需要区分文件）。
stat(path)  → 不打开即可按名称查询；最终符号链接被跟随（lstat 不跟随）。
lstat(path) → 同 stat，但**不跟随**路径最后一个符号链接。
unlink(path)→ 删除名称：目录项被墓碑化（name_len 清零）；已打开文件的内容留到最后一个 handle
              close() 才释放（POSIX unlink-of-open-file 语义）
opendir/readdir/closedir → 真实目录树枚举，包含 POSIX 要求的 "." 与 ".." 目录项。
mkdir/rmdir/chdir/getcwd → 真实目录操作（cwd 参与相对路径解析）。
rename      → 支持文件/目录移动；目录可替换空目录；禁止移入自身子树（-EINVAL）。
truncate/ftruncate → 截断或扩展文件（以 0 填充）。
chmod/fchmod/fchmodat → 存储 mode 位（不强制权限）；fchmodat 支持 AT_SYMLINK_NOFOLLOW，
                        但链接自身的 mode 不可修改（-EOPNOTSUPP）。
symlink/readlink/realpath → 支持相对/绝对符号链接；路径解析自动跟随链接并检测循环。
remove      → 先尝试 unlink，遇目录则尝试 rmdir。
```

设计不变量：

* **文件内容全局共享**：同一路径的多次 `open()` 指向同一 `MemfsNode`（POSIX file-description
  语义，对应真实文件系统对同一 inode 的重复打开）。
* **每次 `open()` 拥有独立偏移**与打开模式。
* **内嵌内容绝不被 realloc/搬运**：写只在 segment0 内原地操作，或追加固定大小 Chunk 节点。
* **名称解析遵循真实 FS**：打包目录里的名称可直接打开；目录里**不存在**的名称必须带
  `O_CREAT` 才能创建（否则 `open` 返回 `-ENOENT`）。`O_TRUNC` 把逻辑大小清零（打包 RAM 保留），
  `O_EXCL|O_CREAT` 遇已存在名称返回 `-EEXIST`。
* **stdin/stdout/stderr 分离**：console 持有 fd 0/1/2，memfs 从 fd 3 起分配文件描述符。

## 2.2 目录语义（真实目录树）

memfs 维护一棵真正的目录树。链接器打包的扁平路径（如 `assets/levels/l1.map`）在 `memfs_init()`
时被展开为目录 `assets`、目录 `levels` 与文件 `l1.map`。运行时创建的目录和文件同样挂在这棵树上。
因此目录不是"前缀分组模拟"，而是有独立节点的真实对象：

* `mkdir("foo")` 创建目录节点 `foo` 作为当前目录（或绝对路径父目录）的子项；调用方传入的 `mode` 会被保存。
* `rmdir("foo")` 删除空目录；非空目录返回 `-ENOTEMPTY`。
* `chdir("foo")` 改变进程的当前工作目录（cwd），并参与后续相对路径解析（`"./bar"`、`"../baz"`、符号链接等）。
* `getcwd()` 返回从 cwd 到根目录的绝对路径。
* `opendir(path)` 返回该目录节点的枚举流；`readdir()` 按 POSIX 约定先返回 `.` 与 `..`，再返回真实子节点。
* `open("a/b/c")` 解析路径时自动跟随中间符号链接并检测循环；路径上的目录节点必须已存在，否则返回 `-ENOENT`。
  带 `O_CREAT` 时只创建最后一个不存在的名称（与真实文件系统一致，不会自动创建中间目录）。

## 3. 目录契约（链接器需生成的镜像）

`tcm_memfs_abi.h` 定义了镜像 ABI（`MemfsPayloadHeader`、`MemfsEntry`、`MEMFS_MAGIC`），链接器与运行时共用。
`tcm_memfs.h` 是其超集（include `tcm_memfs_abi.h` 并追加运行时 BSP API）。链接器生成 payload 布局：

```
offset 0               MemfsPayloadHeader (16 B) 内部私有结构：
  +0  uint32 magic   = MEMFS_MAGIC (0x4D454D46)
  +4  uint32 count   = 目录项数量
  +8  uint32 version = 0
  +12 uint32 data_off= 自 payload 起始到第一个 segment0 的偏移

offset 16              MemfsEntry[count]  每个 20 字节（定长）：
  +0  uint32 name_off      名字在 payload 内的字节偏移（无 NUL 的字节序列起点）
  +4  uint32 name_len      名字字节数
  +8  uint32 seg0_offset   segment0 相对 payload 起始的字节偏移
  +12 uint32 seg0_size     segment0 字节数 = 初始逻辑大小 = 固定容量
  +16 uint32 mode          打开模式位

offset names              各文件名字（按目录项顺序，NUL-free，长度取 name_len）
offset data_off           各文件 segment0 内容（按目录项顺序连续存放）
```

> **名字变长设计**：文件名**不嵌在目录项里**，而是由每个 `MemfsEntry` 用 `name_off/name_len`
> 指向 payload 中的名字区。这样**路径长度不再有硬上限**（可任意深嵌套），贴近真实文件系统；
> 且目录项恒定 20 字节，遍历简单、镜像紧凑。改动此 ABI 需与链接器侧 `--tcm-memfs` 同步。

`start.c:cstart()` 初始化 memfs 时调用的是：

```c
memfs_init();
```

`memfs_init()` 内部通过两个**弱定义函数**读取 payload：

```c
uint8_t *MEMFS_IMAGE(void);      // 返回打包 payload 首地址，无 payload 返回 NULL
uint32_t MEMFS_IMAGE_SIZE(void); // 返回 payload 字节数，无 payload 返回 0
```

链接器 `llvm-link-tcmips --tcm-memfs=<dir>` 应生成**强定义**覆盖这两个函数，
把生成的 `MemfsPayloadHeader + MemfsEntry[] + segment0` 镜像地址/大小暴露给运行时。

当未打包（没有 `--tcm-memfs`）时，`memfs.c` 顶部的弱定义占位保证链接成功，
此时 `MEMFS_IMAGE_SIZE()` 返回 0，`memfs_init()` 最终挂载为一个空目录。

`memfs_init` 对 payload 是**容错**的：payload 缺失 / 过小 / magic 不符时不会失败，而是挂载为一个
**空的 inode 目录**。因此未打包的镜像（`--tcm-memfs` 缺省）同样开机即得到一个可用的空文件系统，
运行时可通过 `open(O_CREAT)` 正常创建/读写新文件。

## 4. 运行时 public API（tcm_memfs.h）

```c
// 初始化。memfs_init() 从链接器覆盖的弱符号读取 payload；
// memfs_init_with_payload() 允许测试/tooling 直接挂载内存 payload。
int  memfs_init(void);
int  memfs_init_with_payload(const void *payload, size_t size); // payload==NULL → 空目录

int  memfs_open_mode(const char *path, int flags, int base_fd,
                     unsigned int mode); // open，带 O_CREAT 时的 mode
int  memfs_open_base(const char *path, int flags, int base_fd); // mode = 0644
int  memfs_close(int fd);
int  memfs_read(int fd, void *buf, size_t nbyte);    // 只写 fd 上返回 -1
int  memfs_write(int fd, const void *buf, size_t nbyte); // 只读 fd 上返回 -1
long memfs_lseek(int fd, long offset, int whence);
int  memfs_stat(int fd, void *statbuf);   // fstat 风格，填 st_size / st_mode

// stat(path) 风格：不打开即按名称查询；返回 0 填 *statbuf，否则负 errno（-ENOENT）
int  memfs_stat_path(const char *path, void *statbuf);
int  memfs_lstat_path(const char *path, void *statbuf); // 不跟随最终符号链接
// unlink(path) 风格：删除名称；有打开句柄时内容留到最后一个 close
int  memfs_unlink(const char *path);
// 目录枚举：handle 对 BSP 透明（cast 为 DIR*）；path 可绝对或相对
void *memfs_opendir(const char *path);   // 失败返回 NULL
int  memfs_readdir(void *dir, void *ent); // 0 填 *ent；-ENOENT 表示枚举结束
int  memfs_closedir(void *dir);
// 真实目录操作（cwd 参与后续相对路径解析）
int memfs_mkdir_mode(const char *path, unsigned int mode);
int memfs_mkdir(const char *path); // convenience, mode = 0755
int memfs_rmdir(const char *path);
int memfs_chdir(const char *path);
char *memfs_getcwd(char *buf, size_t size);
// 额外 POSIX 文件/连接操作（支撑 libc++ <filesystem> 写接口）
int memfs_rename(const char *oldpath, const char *newpath);
int memfs_truncate(const char *path, long length);
int memfs_ftruncate(int fd, long length);
int memfs_chmod(const char *path, unsigned int mode);
int memfs_fchmod(int fd, unsigned int mode);
int memfs_fchmodat(const char *path, unsigned int mode, int flags);
int memfs_symlink(const char *target, const char *linkpath);
ssize_t memfs_readlink(const char *path, char *buf, size_t bufsiz);
char *memfs_realpath(const char *path, char *resolved_path);

// 目录 fd：支撑 libc++ `remove_all` 等非 Windows 平台代码路径。
int memfs_dirfd(void *dir);                 // 为 DIR* 分配一个 synthetic fd
void *memfs_dir_from_fd(int dirfd);         // fd → DIR*
int memfs_dirpath(void *dir, char *buf, size_t sz); // 取 DIR* 的绝对路径
int memfs_openat_dir(int dirfd, const char *name);  // O_DIRECTORY 打开子目录
int memfs_unlinkat(int dirfd, const char *name, int flags); // 0 / AT_REMOVEDIR

long memfs_pathconf(const char *path, int name); // _PC_PATH_MAX / _PC_NAME_MAX

// 调试接口（仅在定义了 TCM_MEMFS_DEBUG 时可用）
#ifdef TCM_MEMFS_DEBUG
void memfs_debug_dump_tree(void);       // 从根开始打印目录树
void memfs_debug_dump_tree_at(const char *path); // 打印某子树
size_t memfs_debug_heap_bytes(void);    // 当前 memfs 内 malloc 字节数
size_t memfs_debug_chunk_count(void);   // 当前 live chunk 数量
#endif
```

BSP 适配（`tcmips/start/stub.c`）把这些接到 picolibc 的 POSIX 层，并保留 fd 0/1/2 的 console 读写：
`open/close/read/write/lseek` → fd 风格；`stat/lstat`→`memfs_stat_path/lstat_path`；
`unlink/rename/chmod/fchmod/fchmodat/truncate/ftruncate/symlink/readlink/realpath/utimes/utimensat/remove`→同名 memfs 函数；
`opendir/readdir/closedir`→memfs 目录枚举（`readdir` 返回 `DIR` 内缓冲 `dd_dirent` 的指针）。
硬链接 `link()` 不被 memfs 的单父目录树支持，stub 直接返回 `EPERM`，libc++ 的硬链接复制会自然回退。

> **前置条件（dirent 类型）**：picolibc 自带的 `<sys/dirent.h>` 是 `#error` 占位，全库没有任何
> `struct dirent` / `DIR` 定义，任何 `#include <dirent.h>` 都会编译失败（libc++ `<filesystem>` 的
> `directory_iterator` 正是靠它）。已在本仓库 vendored 的 picolibc 源
> `tcmips/lib/picolibc-1.8.11/libc/include/sys/dirent.h` 中提供真实类型（`struct dirent`、
> `DT_*`、不透明 `DIR`），并随「源 → sysroot」头同步自动落到 sysroot。

## 5. 构建集成

* `tcmips/lib/libmemfs/CMakeLists.txt` → 静态库 `tcmips_memfs`（`memfs.c`，含默认占位弱符号）。
* `tcmips/lib/CMakeLists.txt` → `add_subdirectory(memfs)`。
* 通过 `tcmips_core` INTERFACE 链接进所有可执行文件。
* `tcm_memfs_abi.h`（镜像布局 ABI）与 `tcm_memfs.h`（运行时 BSP API）位于 `tcmips/include/`，
  均由 `tcm_sync_headers_to_sysroot` 自动同步进 sysroot。

## 6. 测试

`test/memfs/host/test_memfs.c` 是 host 单测（系统编译器直接编译，不依赖 TCMIPS 工具链），手工构造
符合上文契约的 payload，验证：

* 打包文件只读读取 + EOF
* segment0 原地写（打包 RAM 可写）+ 只读 fd 拒绝写（O_ACCMODE 权限）
* 越过 segment0 末尾 → Chunk 链追加 + 读回
* 同一路径多次 open 共享内容（POSIX route A）
* 运行时新建文件：非目录名称不带 `O_CREAT` → 返回 `-ENOENT`；带 `O_CREAT` → 创建可读写
* **空目录**：`memfs_init_with_payload(NULL, 0)` 后，只读打开任意名称 → `-ENOENT`，`O_CREAT` 创建/读写正常
  （对应未打包镜像的可引导空文件系统）
* `memfs_stat` 正确填充 `st_size` 与 `st_mode`（S_IFREG）
* `memfs_stat_path`：打开前/增长后按路径查大小，缺名 → `-ENOENT`
* `memfs_unlink`：打包/运行时/已打开文件都能删，删除后不可复活（目录项墓碑化）
* `memfs_opendir/readdir/closedir`：真实目录树枚举，包含 POSIX 要求的 `.` 与 `..`
* `memfs_mkdir / rmdir / chdir / getcwd`：真实目录操作与相对路径解析，`mkdir` 可接受 mode
* `memfs_rename / truncate / chmod / symlink / readlink / realpath`：符号链接与写类 POSIX 操作
* `memfs_rename` 目录语义：可替换空目录、禁止移入自身子树、`ftruncate` 只读 fd 拒绝截断等
* `memfs_open_mode` / `memfs_openat_dir` / `memfs_dirfd` / `memfs_fdopendir` / `memfs_unlinkat` / `memfs_pathconf`：支撑 libc++ `remove_all` 等代码路径
* `TCM_MEMFS_DEBUG` 内存统计：malloc 字节数、live chunk 数量、truncate 后 chunk 释放、dump 接口不崩溃
* `O_DIRECTORY` / `O_NOFOLLOW` 等 open flag 边界条件，以及路径解析错误码

```sh
# 一键构建并运行普通 + TCM_MEMFS_DEBUG 两个版本
test/memfs/host/run.sh

# 手动普通构建
cc -std=c11 -Itcmips/include test/memfs/host/test_memfs.c tcmips/lib/libmemfs/memfs.c -o /tmp/test_memfs
/tmp/test_memfs

# 手动带 TCM_MEMFS_DEBUG 的构建（会额外编译内存统计与 dump 接口的测试用例）
cc -std=c11 -DTCM_MEMFS_DEBUG -Itcmips/include test/memfs/host/test_memfs.c tcmips/lib/libmemfs/memfs.c -o /tmp/test_memfs_debug
/tmp/test_memfs_debug
```

## 7. 调试与内存统计（TCM_MEMFS_DEBUG）

定义 `TCM_MEMFS_DEBUG`（例如 `cc -DTCM_MEMFS_DEBUG`）后，会额外启用一组调试接口：

* `memfs_debug_dump_tree()` —— 从根目录开始打印当前目录树。
* `memfs_debug_dump_tree_at(path)` —— 打印以 `path` 为根的子树。
* `memfs_debug_heap_bytes()` —— 当前 memfs 内部通过 `malloc` 占用的字节数。
* `memfs_debug_chunk_count()` —— 当前 live 的 `MemfsChunk` 数量。

dump 接口会在树形输出前打印一行：

```
[memfs-debug] heap=<bytes> bytes, chunks=<count>
```

### 7.1 统计的覆盖范围

| 对象 | 是否纳入统计 | 说明 |
|------|-------------|------|
| 节点名 `name` | 是 | `dup_str` / 创建节点时分配 |
| 符号链接目标 `symlink_target` | 是 | `memfs_symlink()` 时分配 |
| 文件内容 Chunk | 是 | 写越 segment0 时分配，truncate 收缩到 segment0 内时会释放尾部 chunk |
| realpath 返回给调用者的 buffer | 否 | 所有权转移给外部，memfs 无法感知外部 free |
| 静态 node / fd / dir-stream 表 | 否 | 这些是编译期固定数组，不经过 malloc |

### 7.2 重复初始化

`memfs_init_with_payload()` 支持作为**全量重置**调用。再次 init 时：

1. 先释放上一个实例的所有堆资源（name、symlink_target、chunk 链）。
2. 清空 node / fd / dir-stream / dirfd 表。
3. 重建空根目录，并按新 payload 建树。

因此再次 init 后，旧的 fd / dir stream 会失效，后续使用返回 `-EBADF`；
但**不会泄漏内存**，也不会因悬空指针崩溃。

### 7.3 使用示例

```c
#define TCM_MEMFS_DEBUG
#include <tcm_memfs.h>

memfs_init_with_payload(NULL, 0);

int fd = memfs_open_base("big.bin", O_WRONLY | O_CREAT, 3);
memfs_write(fd, large_buffer, 8193);      // 触发 chunk 分配
printf("chunks=%zu, heap=%zu\n",
       memfs_debug_chunk_count(),
       memfs_debug_heap_bytes());
memfs_close(fd);
memfs_debug_dump_tree();                  // 打印整棵树 + 统计
```
