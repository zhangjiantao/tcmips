/* * Debug CPU Interpreter — Public Interface
 *
 * The debug interpreter runs the guest one instruction at a time through its
 * own local dispatcher, while a background GDB RSP stub (rsp_stub.c) listens on
 * a TCP port and drives it: reading/writing registers and memory, installing
 * software breakpoints, and issuing continue / single-step.
 *
 * A single global MIPS_CPU instance (g_cpu) is shared with the stub so that
 * register values can be observed and modified while the CPU is stopped.
 */

#ifndef CPU_DEBUG_H
#define CPU_DEBUG_H

#include <stdatomic.h>

#include "cpu.h"

/* Debug interpreter instance (vtable, as declared in cpu.h). */
extern const cpu_interpreter_t g_interp_debug;

/* The MIPS_CPU state the debug interpreter executes on and exposes to the RSP
 * stub.  Only safe to read/write while the run gate is closed (see
 * g_dbg_pause_requested), but not necessarily yet signalled to the stub —
 * wait for g_dbg_stop_event for a fresh stop. */
extern MIPS_CPU g_debug_cpu;

/* ── Run / stop coordination with the RSP stub thread ─────────────── */

/* The debug interpreter clears these at the start of a run; the stub thread
 * sets/clears them to steer execution.  All are atomic.
 *
 * g_dbg_pause_requested acts as the run-gate: true = CPU may run, false = CPU
 * is stopped and waiting.  g_dbg_single_step, when set together with the gate
 * open, requests exactly one instruction followed by another stop. */
extern atomic_bool g_dbg_pause_requested; /* gate: true = run, false = stop */
extern atomic_bool g_dbg_single_step;     /* run exactly one instruction */

/* Stop event — cleared by the stub before requesting a run, set by the CPU
 * thread the moment it has actually halted (run gate closed, breakpoint hit,
 * single step done, or interrupt).  The stub waits on this becoming true
 * before sending its stop reply, so that the observed CPU state and the
 * reported reason are consistent and not stale from a previous stop cycle. */
extern atomic_bool g_dbg_stop_event;

/* Exit handshake, one variable both directions: 0 = guest running, 1 =
 * guest finished for good (BREAK(0x3ff)) — the stub then reports a proper
 * process-exit stop reply ('W<exit-code>') instead of dropping the
 * connection — and 2 = the 'W' packet was sent.  cpu_debug waits (bounded)
 * for phase 2 before tearing the stub down so the reply is flushed rather
 * than cut off by the socket close. */
extern atomic_uint g_dbg_exit_phase;


/* Install or remove a software breakpoint at a guest address.  The
 * interpreter never patches guest memory; it keeps a private table of
 * breakpoint addresses and checks PC against it before each instruction.
 * @return 0 on success, non-zero on failure. */
int cpu_debug_set_breakpoint(uint32_t addr);
int cpu_debug_remove_breakpoint(uint32_t addr);


/* Guest exit code captured by the debug interpreter's r_break handler; used
 * by rsp_stub.c for the 'W<exit-code>' process-exit stop reply. */
uint32_t cpu_debug_get_exit_code(void);

#endif /* CPU_DEBUG_H */