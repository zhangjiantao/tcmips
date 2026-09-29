/*
 * JIT CPU Interpreter — Public Interface
 *
 * Declares the JIT interpreter instance.  The interpreter-vtable type is
 * defined in cpu.h; both the fast (cpu_fast.c) and debug (cpu_debug.c)
 * interpreters expose an instance of the same vtable so host.c can choose
 * between them without knowing any implementation details.
 *
 * The JIT interpreter is built on SLJIT (a CPU-independent, stack-less
 * compiler).  A single C source translates guest MIPS instructions into
 * native x86-64 / AArch64 code at run time, so the same backend covers both
 * host architectures with no platform-specific machine-code emission.
 */

#ifndef CPU_JIT_H
#define CPU_JIT_H

#include "cpu.h"

/* JIT interpreter instance (SLJIT-based, native code). */
extern const cpu_interpreter_t g_interp_jit;

#endif /* CPU_JIT_H */
