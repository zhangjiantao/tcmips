/* * GDB Remote Serial Protocol (RSP) Stub — Interface
 *
 * Runs a TCP server on localhost that speaks the *server* side of the GDB
 * Remote Serial Protocol, so a host debugger (gdb / lldb) can connect and drive
 * the TCMIPS debug interpreter.
 *
 * The stub runs on its own platform thread while the debug interpreter runs the
 * guest on the CPU thread.  They coordinate through the atomic control flags in
 * cpu_debug.h and through a small lock guarding register / memory access.
 *
 * Supported packet subsets:
 *   presence/hello  : qSupported, vMustReplyEmpty, packet-length, qAttached
 *                     qProcessInfo, qThreadInfo, qXfer:features (minimal tdesc),
 *                     qTStatus, qThreadExtraInfo
 *   stop reason      : "?"
 *   registers        : g / G / p / P
 *   memory           : m / M
 *   execution        : c / s / vCont;c / vCont;s (+ optional address)
 *   breakpoints      : Z0 / z0 (software)
 *   control          : ^C (interrupt), k (kill), D (detach), H (thread)
 */

#ifndef RSP_STUB_H
#define RSP_STUB_H

#include <stdint.h>

/** Start the RSP server thread listening on @p port (0 = default 1234).
 *  @return 0 on success, non-zero on failure. */
int rsp_stub_start(int port);

/** Stop the server: close the listening socket and any active connection and
 *  detach the background thread. */
void rsp_stub_stop(void);

#endif /* RSP_STUB_H */