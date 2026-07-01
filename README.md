<p align="center">
  <img src="docs/images/LOGO_TCMIPS_trim.webp"
       style="image-rendering: pixelated; image-rendering: crisp-edges;"
       alt="TCMIPS Logo">
</p>

## TCMIPS

This repository hosts the full TCMIPS ecosystem, including TCMIPS CPU architecture game save files, the `tcmips_core`
library, and ported `picolibc` and `libc++` standard libraries, along with a complete C/C++ development toolchain built
for the `Turing Complete` sandbox simulator.

It integrates a modified Clang compiler and a custom linker to compile standard C/C++ source code into fully linked
`.tcm` binaries. These compiled .tcm binaries can be loaded and executed directly via the in-game File Loader on the
imported TCMIPS CPU architecture, which natively supports peripherals such as input devices, pixel/ascii consoles, 
and seven-segment displays.

## Features

* **Modified Clang Compiler**: An LLVM/Clang compiler tailored for the TCMIPS architecture, supporting its custom
  instruction encodings, register allocation scheme and pipeline design.
* **Custom Linker (`llvm-link-tcmips`)**: A dedicated linker that resolves object files and outputs `.tcm` executables
  formatted for the in-game File Loader environment.
* **Integrated Core Library (`tcmips_core`)**: A foundational runtime library integrating ported picolibc, libc++ and
  compiler-rt, providing standard C/C++ runtime features and memory allocation for the virtual hardware.
* **Software Arithmetic Support**: Full software floating-point emulation and 64‑bit integer operation support via
  compiler-rt, enabling complex computations on custom virtual hardware.
* **Dedicated I/O Syscall Interface**: Built-in architecture support for in-game peripherals through dedicated MIPS
  syscalls, including sound, keyboard, display console and seven-segment display array.

## Repository Structure

- `/tcmips` — Core library integrating runtime libraries and peripheral drivers.
- `/toolchains` — Prebuilt cross-toolchain, CMake toolchain file and sysroot.
- `/tools` — Python scripts.
- `/test` — Test cases.
- `/docs` — Architecture documents: ISA, memory layout, boot flow.
- `/demo` — Sample demos.
- `/gamesave` — Game save files for TCMIPS architecture.

## Demo

- Booting Win95 via tiny386

  <img src="docs/images/win95.webp" width="50%">
  <img src="docs/images/win952.webp" width="50%">

---

- Booting Macintosh via umac

  <img src="docs/images/macintosh.webp" width="50%">

---

- Doom

  <img src="docs/images/Doom.webp" width="50%">

---

- SDLPAL

  <img src="docs/images/SDLPAL.jpg" width="50%">


More Demos:
https://space.bilibili.com/28801454/lists/8407732

## Technology Stack

```mermaid
%%{init: {'theme': 'neutral'}}%%
flowchart LR
    subgraph HardwareBox ["Hardware Infrastructure"]
        CpuCore[["Processor Core (CPU)"]]
        Syscall[["Syscall Controller"]]
        KbdCtrl[["Keyboard Controller"]]
        ConCtrl[["Console Controller"]]
        HwOthers["Etc."]

        CpuCore --- Syscall --- KbdCtrl --- ConCtrl --- HwOthers
    end

    subgraph ToolchainBox ["Compilation Toolchain"]
        Clang["Custom Clang Compiler<br>(Code Generation & Frontend)"]
        Linker["Custom llvm-link-tcmips Linker<br>(LTO & TCMIPS Memory Mapping)"]

        Clang --- Linker
    end

    subgraph SoftwareBox ["Runtime Software Stack"]
        libcxx["libcxx / libcxxabi<br>(C++ Standard Library & Runtime)"]
        picolibc["picolibc<br>(Embedded Standard C Library)"]
        compiler_rt["libcompiler_rt<br>(Low-Level Compiler Runtime)"]
        bsp_drivers["Hardware Drivers (BSP)<br>(Keyboard, VRAM & Storage Drivers)"]

        libcxx ---> picolibc ---> compiler_rt ---> bsp_drivers
    end

    ToolchainBox -.->|Builds & Optimizes| SoftwareBox
    SoftwareBox -.->|Drives & Controls| HardwareBox

    style HardwareBox fill:#f5f5f5,stroke:#d9d9d9,stroke-width:2px,color:#333
    style ToolchainBox fill:#f6ffed,stroke:#52c41a,stroke-width:2px,color:#333
    style SoftwareBox fill:#e6f7ff,stroke:#1890ff,stroke-width:2px,color:#333

    style Clang fill:#ffffff,stroke:#52c41a,stroke-width:1px
    style Linker fill:#ffffff,stroke:#52c41a,stroke-width:1px

    style CpuCore fill:#fff1f0,stroke:#f5222d,color:#322
    style Syscall fill:#fff1f0,stroke:#f5222d,color:#322
    style KbdCtrl fill:#fff1f0,stroke:#f5222d,color:#322
    style ConCtrl fill:#fff1f0,stroke:#f5222d,color:#322
    style HwOthers fill:#ffffff,stroke:#d9d9d9,stroke-dasharray: 3 3,color:#8c8c8c
```



## Build Toolchain and Sysroot

**Note: You can skip this section if you use the prebuilt toolchain from the project Release page.**

### 1. Build LLVM

Build the customized LLVM toolchain by referring to this repository:

[https://github.com/zhangjiantao/tcmips-llvm](https://github.com/zhangjiantao/tcmips-llvm)

### 2. Deploy LLVM Build Artifacts

```shell
# Create target toolchain directory
mkdir -p ./toolchains/llvm/bin

# Copy core compiler and linker
cp ${LLVM_BUILD_PATH}/bin/clang ./toolchains/llvm/bin/
cp ${LLVM_BUILD_PATH}/bin/clang++ ./toolchains/llvm/bin/
cp ${LLVM_BUILD_PATH}/bin/llvm-link-tcmips ./toolchains/llvm/bin/

# Grant executable permission
chmod +x ./toolchains/llvm/bin/*
```

### 3. Build Sysroot

Construct a minimal sysroot environment, which provides target platform header files, static libraries for cross-compilation.

```shell
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build --target tcmips_cores_all
```

## Build Your C/C++ Project

```shell
cd ${YOUR_PROJECT_ROOT}

cmake -G Ninja -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=${TCMIPS_PROJECT_ROOT}/toolchains/cmake/tcmips.toolchain.cmake

cmake --build build

```

**Note:** Replace `${TCMIPS_PROJECT_ROOT}` with `this project` root path.

The compiled .tcm file can be loaded and executed directly.

## Related Repositories

* [https://github.com/zhangjiantao/tcmips-llvm](https://github.com/zhangjiantao/tcmips-llvm)

