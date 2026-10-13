/* * Fast CPU Interpreter — Public Interface
 *
 * Declares the fast interpreter instance.  The interpreter-vtable type is
 * defined in cpu.h; other interpreter implementations (e.g. the debug
 * interpreter) provide an instance of the same vtable so that host.c can choose
 * between them without knowing any implementation details.
 */

#ifndef CPU_FAST_H
#define CPU_FAST_H

#include "cpu.h"

/* Fast interpreter instance (computed-goto dispatch). */
extern const cpu_interpreter_t g_interp_fast;

/* Clamp the fast interpreter's execution speed to @p mhz MHz (0 = unlimited).
 * Must be called before cpu_fast_execute() runs. Only the fast interpreter
 * honours this; jit/debug ignore it. */
void cpu_fast_set_speed_mhz(uint32_t mhz);

#endif /* CPU_FAST_H */
