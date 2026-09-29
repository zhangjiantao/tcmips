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

#endif /* CPU_FAST_H */
