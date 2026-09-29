/* * GDB Remote Serial Protocol (RSP) Stub — Implementation
 *
 * A small, self-contained TCP server implementing the *server* side of the GDB
 * Remote Serial Protocol so a host debugger (gdb / lldb) can connect and drive
 * the TCMIPS debug interpreter.  The stub runs on its own platform thread while
 * the debug interpreter runs the guest on the CPU thread.  Registers and guest
 * memory are only meaningfully accessed while the CPU is reported stopped.
 *
 * Register file layout (little-endian, 32-bit registers):
 *   0-31  GPR r0..r31       32  SR (status)   33  LO   34  HI
 *   35    BADVADDR          36  CAUSE         37  PC
 * The g / G / p / P packets use byte offsets into this file and the target
 * description returned by qXfer matches it.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_debug.h"
#include "host.h"
#include "memory.h"
#include "platform.h"
#include "rsp_stub.h"

/* ════════════════════════════════════════════════════════════════════ */
/*  Platform sockets                                                     */
/* ════════════════════════════════════════════════════════════════════ */

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define SOCK_CLOSE closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define SOCK_CLOSE close
#endif

/* ════════════════════════════════════════════════════════════════════ */
/*  Server state                                                         */
/* ════════════════════════════════════════════════════════════════════ */

#define RSP_DEFAULT_PORT 1234
#define RSP_BUF_MAX 2048
#define RSP_PACKET_SIZE_HEX "800" /* max packet payload, hex = 2048 */

/* ════════════════════════════════════════════════════════════════════ */
/*  Debug logging — controlled by RSP_STUB_DEBUG                         */
/*                                                                       */
/*  When enabled, every command/packet (and every control byte) received  */
/*  from the debugger is dumped to stderr.  stderr is used on purpose so  */
/*  the semihost stdout channel stays reserved for guest output.  Build   */
/*  with -DRSP_STUB_DEBUG, or see set(RSP_STUB_DEBUG 1) in CMakeLists.txt */
/*  which currently enables it by default.                                */
/* ════════════════════════════════════════════════════════════════════ */
#ifdef RSP_STUB_DEBUG
#define RSP_LOG_IN(...)                                                        \
  do {                                                                         \
    fprintf(stderr, "[rsp-stub] " __VA_ARGS__);                                \
    fflush(stderr); /* stderr is fully buffered when piped; without the        \
                     * flush, TerminateProcess-style kills lose the log */     \
  } while (0)
#else
#define RSP_LOG_IN(...) ((void)0)
#endif

static platform_thread_t g_stub_thread;
static bool g_thread_started = false;

static sock_t g_listen_fd = SOCK_INVALID;
static sock_t g_client_fd = SOCK_INVALID;

static volatile bool g_stub_shutdown = false;

#if defined(_WIN32) || defined(_WIN64)
/* Winsock must be initialised (WSAStartup) before socket()/bind()/accept()
 * work; without it socket() returns INVALID_SOCKET on Windows. */
static bool g_wsa_inited = false;
#endif

/* ════════════════════════════════════════════════════════════════════ */
/*  Hex serialization helpers                                            */
/* ════════════════════════════════════════════════════════════════════ */

static int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static void hex_to_bytes(const char *hex, uint8_t *out, int nbytes) {
  for (int i = 0; i < nbytes; i++) {
    int hi = hex_val(hex[2 * i]);
    int lo = hex_val(hex[2 * i + 1]);
    out[i] = (uint8_t)(((hi & 0xF) << 4) | (lo & 0xF));
  }
}

static void bytes_to_hex(const uint8_t *in, int nbytes, char *out) {
  static const char HEX[] = "0123456789abcdef";
  for (int i = 0; i < nbytes; i++) {
    out[2 * i] = HEX[(in[i] >> 4) & 0xF];
    out[2 * i + 1] = HEX[in[i] & 0xF];
  }
  out[2 * nbytes] = '\0';
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Packet framing                                                       */
/* ════════════════════════════════════════════════════════════════════ */

static uint8_t rsp_checksum(const char *payload, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += (uint8_t)payload[i];
  return sum;
}

/* Read one '$'-delimited packet (including checksum) into buf.  Also accepts
 * '+'/'-' acks and the 0x03 interrupt byte.  Returns:
 *   1  full packet read into buf (payload, NUL-terminated; checksum verified)
 *   0  an ack (ctrl = '+', '-', or 0x03)
 *  -1  connection closed or checksum mismatch.
 * A bad checksum is treated as a protocol error and causes the whole
 * connection to be closed; no NACK is sent. */
static int rsp_read_packet(sock_t fd, char *buf, size_t buf_size, int *ctrl) {
  size_t n = 0;
  int in_packet = 0; /* 0 idle, 1 after '$', 2 collecting (checksum) */
  uint8_t calc = 0;  /* running checksum of payload */
  int cksum_hi = -1; /* collected first checksum digit (-1 = none) */

  for (;;) {
    char byte;
    int r = (int)recv(fd, &byte, 1, 0);
    if (r <= 0)
      return -1;
    uint8_t b = (uint8_t)byte;

    if (!in_packet) {
      if (b == '$') {
        in_packet = 1;
        n = 0;
        calc = 0;
        cksum_hi = -1;
      } else if (b == '+') {
        RSP_LOG_IN("<-- ack '+'\n");
        *ctrl = '+';
        return 0;
      } else if (b == '-') {
        RSP_LOG_IN("<-- nack '-'\n");
        *ctrl = '-';
        return 0;
      } else if (b == 0x03) {
        RSP_LOG_IN("<-- interrupt 0x03\n");
        *ctrl = 0x03;
        return 0;
      }
      /* ignore other bytes */
      continue;
    }

    if (in_packet == 1) {
      if (b == '#') {
        in_packet = 2;
      } else if (n < buf_size - 1) {
        buf[n++] = (char)b;
        calc += b;
      } else {
        return -1; /* overflow */
      }
      continue;
    }

    /* in_packet == 2: checksum digits */
    int v = hex_val((char)b);
    if (v < 0)
      return -1;
    if (cksum_hi < 0) {
      cksum_hi = v;
      continue;
    }
    uint8_t sent = (uint8_t)((cksum_hi << 4) | v);
    buf[n] = '\0';
    if (calc == sent) {
      RSP_LOG_IN("<-- command \"%s\" (%zu bytes payload)\n", buf, n);
      return 1;
    }
    return -1; /* bad checksum → treat as protocol error */
  }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Register file access                                                 */
/* ════════════════════════════════════════════════════════════════════ */

#define RSP_NUM_REGS 38

/* SR / BADVADDR / CAUSE — no dedicated guest storage (software-only). */
static uint32_t g_sr = 0, g_badvaddr = 0, g_cause = 0;

static uint32_t rsp_reg_read(int idx) {
  if (idx >= 0 && idx < 32)
    return (idx == 0) ? 0 : g_debug_cpu.GPR[idx];
  switch (idx) {
  case 32:
    return g_sr;
  case 33:
    return g_debug_cpu.LO;
  case 34:
    return g_debug_cpu.HI;
  case 35:
    return g_badvaddr;
  case 36:
    return g_cause;
  case 37:
    return g_debug_cpu.PC;
  default:
    return 0;
  }
}

static bool rsp_reg_write(int idx, uint32_t val) {
  if (idx == 0)
    return false;
  if (idx >= 1 && idx < 32) {
    g_debug_cpu.GPR[idx] = val;
    return true;
  }
  switch (idx) {
  case 32:
    g_sr = val;
    return true;
  case 33:
    g_debug_cpu.LO = val;
    return true;
  case 34:
    g_debug_cpu.HI = val;
    return true;
  case 35:
    g_badvaddr = val;
    return true;
  case 36:
    g_cause = val;
    return true;
  case 37:
    g_debug_cpu.PC = val;
    return true;
  default:
    return false;
  }
}

static void rsp_fill_gimg(uint8_t *img) {
  for (int i = 0; i < RSP_NUM_REGS; i++) {
    uint32_t v = rsp_reg_read(i);
    memcpy(img + 4 * i, &v, 4);
  }
}

static void rsp_apply_gimg(const uint8_t *img) {
  for (int i = 0; i < RSP_NUM_REGS; i++) {
    uint32_t v;
    memcpy(&v, img + 4 * i, 4);
    rsp_reg_write(i, v);
  }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Memory access                                                        */
/* ════════════════════════════════════════════════════════════════════ */

static bool in_guest_mem(uint64_t addr, uint64_t len) {
  return addr < TCM_MEMORY_SIZE && len <= TCM_MEMORY_SIZE &&
         addr + len <= TCM_MEMORY_SIZE;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Software breakpoints (Z0 / z0)                                      */
/* ════════════════════════════════════════════════════════════════════ */

/* Breakpoints are modelled as a pure address table held by the debug
 * interpreter (cpu_debug.c).  The guest image is never patched, so memory
 * reads always expose the original instruction and single-stepping over a
 * breakpoint works naturally. */

/* ════════════════════════════════════════════════════════════════════ */
/*  Target description (qXfer:features:read)                             */
/* ════════════════════════════════════════════════════════════════════ */

static const char *const g_tdesc =
    "<?xml version=\"1.0\"?>"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
    "<target>"
    "<architecture>mips</architecture>"
    "<osabi>none</osabi>"
    "<reg name=\"r0\" bitsize=\"32\"/>"
    "<reg name=\"r1\" bitsize=\"32\"/>"
    "<reg name=\"r2\" bitsize=\"32\"/>"
    "<reg name=\"r3\" bitsize=\"32\"/>"
    "<reg name=\"r4\" bitsize=\"32\"/>"
    "<reg name=\"r5\" bitsize=\"32\"/>"
    "<reg name=\"r6\" bitsize=\"32\"/>"
    "<reg name=\"r7\" bitsize=\"32\"/>"
    "<reg name=\"r8\" bitsize=\"32\"/>"
    "<reg name=\"r9\" bitsize=\"32\"/>"
    "<reg name=\"r10\" bitsize=\"32\"/>"
    "<reg name=\"r11\" bitsize=\"32\"/>"
    "<reg name=\"r12\" bitsize=\"32\"/>"
    "<reg name=\"r13\" bitsize=\"32\"/>"
    "<reg name=\"r14\" bitsize=\"32\"/>"
    "<reg name=\"r15\" bitsize=\"32\"/>"
    "<reg name=\"r16\" bitsize=\"32\"/>"
    "<reg name=\"r17\" bitsize=\"32\"/>"
    "<reg name=\"r18\" bitsize=\"32\"/>"
    "<reg name=\"r19\" bitsize=\"32\"/>"
    "<reg name=\"r20\" bitsize=\"32\"/>"
    "<reg name=\"r21\" bitsize=\"32\"/>"
    "<reg name=\"r22\" bitsize=\"32\"/>"
    "<reg name=\"r23\" bitsize=\"32\"/>"
    "<reg name=\"r24\" bitsize=\"32\"/>"
    "<reg name=\"r25\" bitsize=\"32\"/>"
    "<reg name=\"r26\" bitsize=\"32\"/>"
    "<reg name=\"r27\" bitsize=\"32\"/>"
    "<reg name=\"r28\" bitsize=\"32\"/>"
    "<reg name=\"r29\" bitsize=\"32\"/>"
    "<reg name=\"r30\" bitsize=\"32\"/>"
    "<reg name=\"r31\" bitsize=\"32\"/>"
    "<reg name=\"sr\" bitsize=\"32\"/>"
    "<reg name=\"lo\" bitsize=\"32\"/>"
    "<reg name=\"hi\" bitsize=\"32\"/>"
    "<reg name=\"bad\" bitsize=\"32\"/>"
    "<reg name=\"cause\" bitsize=\"32\"/>"
    "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
    "</target>";

/* ════════════════════════════════════════════════════════════════════ */
/*  Send helpers                                                         */
/* ════════════════════════════════════════════════════════════════════ */

static void rsp_send_packet(sock_t fd, const char *payload, size_t len) {
  char frame[RSP_BUF_MAX + 16];
  size_t off = 0;
  frame[off++] = '$';
  memcpy(frame + off, payload, len);
  off += len;
  frame[off++] = '#';
  uint8_t cs = rsp_checksum(payload, len);
  char csbuf[4];
  sprintf(csbuf, "%02x", cs);
  frame[off++] = csbuf[0];
  frame[off++] = csbuf[1];
  send(fd, frame, (int)off, 0);
}

static void rsp_send_str(sock_t fd, const char *s) {
  rsp_send_packet(fd, s, strlen(s));
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Execution-control helpers (shared with cpu_debug)                    */
/* ════════════════════════════════════════════════════════════════════ */

static void rsp_continue(void) {
  /* Arm a fresh stop cycle, then open the run-gate. */
  atomic_store_explicit(&g_dbg_stop_event, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_pause_requested, true, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_single_step, false, memory_order_relaxed);
}

static void rsp_step(void) {
  /* Arm a fresh stop cycle, open the run-gate, ask for one instruction. */
  atomic_store_explicit(&g_dbg_stop_event, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_pause_requested, true, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_single_step, true, memory_order_relaxed);
}

static void rsp_interrupt(void) {
  /* Close the run-gate: the CPU should stop as soon as possible. */
  atomic_store_explicit(&g_dbg_pause_requested, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_single_step, false, memory_order_relaxed);
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Stop-reply synchronization                                            */
/* ════════════════════════════════════════════════════════════════════ */

/* Receive 1 byte with a timeout: 1 = got byte, 0 = timeout, -1 = closed. */
static int rsp_recv_byte_timeout(sock_t fd, uint8_t *out, int timeout_ms) {
  fd_set rf;
  FD_ZERO(&rf);
  FD_SET(fd, &rf);
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  int rc = select((int)fd + 1, &rf, NULL, NULL, &tv);
  if (rc == 0)
    return 0;
  if (rc < 0)
    return -1;
  int n = (int)recv(fd, (char *)out, 1, 0);
  if (n <= 0)
    return -1;
  return 1;
}

/* Run the CPU (flags already set by rsp_continue / rsp_step) and send the
 * stop reply once it actually halts.  Per the GDB RSP the *reply* to c / s /
 * vCont IS the stop reply — the debugger blocks on it — so we must not
 * answer these packets with "OK" as soon as they arrive.
 *
 * While the guest runs we still service raw 0x03 interrupt bytes so the
 * client can break into a long-running guest.  Any other byte the client
 * sends in this window is discarded (per RSP it should not send packets
 * while the target is running).  On emulator shutdown (guest terminated /
 * teardown) we return without a reply. */
static void rsp_run_and_report_stop(sock_t fd) {
  while (!g_stub_shutdown) {
    if (atomic_load_explicit(&g_dbg_exit_phase, memory_order_relaxed) == 1) {
      /* Guest finished (BREAK(0x3ff)): report the process exit properly
       * ('W<exit-code-hex>') instead of dropping the connection — GDB then
       * shows "Program exited normally" instead of "Remote closed". */
      char buf[8];
      snprintf(buf, sizeof buf, "W%02x",
               (unsigned)(cpu_debug_get_exit_code() & 0xFF));
      rsp_send_str(fd, buf);
      atomic_store_explicit(&g_dbg_exit_phase, 2, memory_order_relaxed);
      return;
    }
    if (!host_is_running())
      return; /* torn down without a clean guest exit, no stop reply */
    if (atomic_load_explicit(&g_dbg_stop_event, memory_order_relaxed)) {
      rsp_send_str(fd, "S05"); /* SIGTRAP: breakpoint / step / interrupt */
      return;
    }
    /* Poll the socket for ^C / stray bytes.  The timeout doubles as the
     * stop-event detection granularity: the CPU thread sets g_dbg_stop_event
     * when it halts, and we only discover it here once select() times out.
     * 5 ms keeps GDB single-stepping snappy (worst-case +5 ms per step)
     * while the idle-time cost of waking every 5 ms is negligible. */
    uint8_t b = 0;
    int r = rsp_recv_byte_timeout(fd, &b, 5);
    if (r < 0)
      return; /* connection closed */
    if (r == 1 && b == 0x03) {
      RSP_LOG_IN("<-- interrupt 0x03 (while running)\n");
      rsp_interrupt(); /* CPU stops on its next loop check */
    } else if (r == 1) {
      RSP_LOG_IN("<-- stray byte 0x%02x (while running)\n", b);
    }
  }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Memory / breakpoint handlers                                         */
/* ════════════════════════════════════════════════════════════════════ */

static uint64_t rsp_parse_addr(const char *p, const char **rest) {
  uint64_t v = 0;
  int d;
  while ((d = hex_val(*p)) >= 0) {
    v = (v << 4) | (uint32_t)d;
    p++;
  }
  *rest = p;
  return v;
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Packet dispatch                                                      */
/* ════════════════════════════════════════════════════════════════════ */

static void rsp_handle(const char *p, sock_t fd) {
  char cmd = p[0];

  switch (cmd) {
  case '?':
    rsp_send_str(fd, "S05"); /* stopped on break/step */
    return;

  case 'g': {
    uint8_t img[RSP_NUM_REGS * 4];
    char out[RSP_NUM_REGS * 16 + 1];
    fflush(stderr);
    rsp_fill_gimg(img);
    bytes_to_hex(img, RSP_NUM_REGS * 4, out);
    rsp_send_str(fd, out);
    return;
  }

  case 'G': {
    uint8_t img[RSP_NUM_REGS * 4];
    hex_to_bytes(p + 1, img, RSP_NUM_REGS * 4);
    rsp_apply_gimg(img);
    rsp_send_str(fd, "OK");
    return;
  }

  case 'p': {
    const char *rest = NULL;
    uint64_t idx = rsp_parse_addr(p + 1, &rest);
    if (idx >= RSP_NUM_REGS) {
      rsp_send_str(fd, "E02");
      return;
    }
    uint32_t v = rsp_reg_read((int)idx);
    char hex[9];
    bytes_to_hex((const uint8_t *)&v, 4, hex);
    rsp_send_str(fd, hex);
    return;
  }

  case 'P': {
    const char *rest = NULL;
    uint64_t idx = rsp_parse_addr(p + 1, &rest);
    if (rest[0] != '=' || idx >= RSP_NUM_REGS) {
      rsp_send_str(fd, "E02");
      return;
    }
    uint8_t buf[4];
    hex_to_bytes(rest + 1, buf, 4);
    uint32_t v;
    memcpy(&v, buf, 4);
    if (!rsp_reg_write((int)idx, v)) {
      rsp_send_str(fd, "E03");
      return;
    }
    rsp_send_str(fd, "OK");
    return;
  }

  case 'm': {
    const char *rest = NULL;
    uint64_t addr = rsp_parse_addr(p + 1, &rest);
    if (rest[0] != ',') {
      rsp_send_str(fd, "E01");
      return;
    }
    uint64_t len = rsp_parse_addr(rest + 1, &rest);
    if (!in_guest_mem(addr, len) || len > RSP_BUF_MAX / 2) {
      rsp_send_str(fd, "E01");
      return;
    }
    char hex[RSP_BUF_MAX + 1];
    bytes_to_hex(g_emu_memory + addr, (int)len, hex);
    rsp_send_str(fd, hex);
    return;
  }

  case 'M': {
    const char *rest = NULL;
    uint64_t addr = rsp_parse_addr(p + 1, &rest);
    if (rest[0] != ',') {
      rsp_send_str(fd, "E01");
      return;
    }
    uint64_t len = rsp_parse_addr(rest + 1, &rest);
    if (rest[0] != ':' || !in_guest_mem(addr, len)) {
      rsp_send_str(fd, "E01");
      return;
    }
    uint8_t buf[RSP_BUF_MAX / 2];
    hex_to_bytes(rest + 1, buf, (int)len);
    memcpy(g_emu_memory + addr, buf, len);
    rsp_send_str(fd, "OK");
    return;
  }

  case 'Z':
  case 'z': {
    /* Only kind '0' (software) supported. */
    if (p[1] != '0') {
      rsp_send_str(fd, "");
      return;
    }
    const char *rest = NULL;
    uint64_t addr = rsp_parse_addr(p + 3, &rest); /* skip "Z0," */
    if (rest[0] != ',') {
      rsp_send_str(fd, "");
      return;
    }
    if (cmd == 'Z') {
      rsp_send_str(fd, cpu_debug_set_breakpoint((uint32_t)addr) == 0 ? "OK"
                                                                     : "E04");
    } else {
      rsp_send_str(fd, cpu_debug_remove_breakpoint((uint32_t)addr) == 0 ? "OK"
                                                                        : "");
    }
    return;
  }

  case 'c':
    rsp_continue();
    rsp_run_and_report_stop(fd); /* RSP: the c reply IS the stop reply */
    return;

  case 's':
    rsp_step();
    rsp_run_and_report_stop(fd); /* RSP: the s reply IS the stop reply */
    return;

  case 'v': {
    if (strncmp(p, "vMustReplyEmpty", 15) == 0) {
      rsp_send_str(fd, "");
      return;
    }
    if (strncmp(p, "vCont?", 6) == 0) {
      /* Capability probe from the debugger: report WHICH vCont actions we
       * implement.  This must never start the CPU — it is a query, not a
       * resume request (a plain prefix match on "vCont" used to swallow it
       * and silently continue the guest). */
      rsp_send_str(fd, "vCont;c;s");
      return;
    }
    if (strncmp(p, "vCont", 5) == 0) {
      /* Grammar (GDB RSP): vCont[;action[:thread-id]]...
       * Supported actions: 'c' (continue), 'C<sig>' (continue w/ signal),
       * 's' (step), 'S<sig>' (step w/ signal); ',':thread is a hex ID which
       * we parse over (single-thread guest).  Later actions in the list
       * take precedence over earlier ones.  A bare "vCont", an empty action
       * list, or any unknown action (e.g. 't') is answered with an EMPTY
       * packet so GDB falls back to plain s / c — it must NOT resume the
       * CPU (older code silently continued on exactly those inputs). */
      bool step = false;
      bool have_action = false;
      const char *a = p + 5;
      while (*a) {
        if (*a != ';')
          break;
        a++;
        char act = *a;
        if (act == '\0')
          break; /* trailing ';' with no action → reject */
        if (act == 'c' || act == 'C') {
          step = false;
          have_action = true;
        } else if (act == 's' || act == 'S') {
          step = true;
          have_action = true;
        } else {
          have_action = false; /* unknown action → unsupported */
          break;
        }
        /* skip optional ':thread-id' up to the next ';' / end */
        while (*a && *a != ';')
          a++;
      }
      if (!have_action) {
        rsp_send_str(fd, "");
        return;
      }
      if (step)
        rsp_step();
      else
        rsp_continue();
      rsp_run_and_report_stop(fd); /* RSP: the vCont reply IS the stop reply */
      return;
    }
    rsp_send_str(fd, "");
    return;
  }

  case 'q': {
    if (strncmp(p, "qSupported", 10) == 0) {
      char resp[128];
      snprintf(resp, sizeof(resp), "PacketSize=%s;qXfer:features:read+;",
               RSP_PACKET_SIZE_HEX);
      rsp_send_str(fd, resp);
      return;
    }
    if (strncmp(p, "qXfer:features:read:", 20) == 0) {
      /* Format: qXfer:features:read:ANNEX:OFFSET:LEN.  Skip the annex field.
       * GDB separates OFFSET from LEN with ',' (e.g. ...target.xml:0,1fb). */
      const char *rest = strchr(p + 20, ':');
      if (!rest) {
        rsp_send_str(fd, "E");
        return;
      }
      uint64_t off = rsp_parse_addr(rest + 1, &rest);
      if (rest[0] != ',') {
        rsp_send_str(fd, "E");
        return;
      }
      uint64_t want = rsp_parse_addr(rest + 1, &rest);
      size_t total = strlen(g_tdesc);
      if (off >= total) {
        rsp_send_packet(fd, "l", 1);
        return;
      }
      const char *body = g_tdesc + off;
      size_t avail = total - off;
      size_t take = (want < avail) ? (size_t)want : avail;
      char resp[RSP_BUF_MAX + 2];
      size_t rlen = 0;
      resp[rlen++] = (off + take < total) ? 'm' : 'l';
      memcpy(resp + rlen, body, take);
      rlen += take;
      rsp_send_packet(fd, resp, rlen);
      return;
    }
    if (strncmp(p, "qAttached", 9) == 0) {
      rsp_send_str(fd, "0");
      return;
    }
    if (strncmp(p, "qTStatus", 8) == 0) {
      rsp_send_str(fd, "");
      return;
    }
    if (strncmp(p, "qOffsets", 8) == 0) {
      /* Where the guest image's .text/.data/.bss really landed.  The values
       * are additive relocations on top of the symfile's own section VMAs
       * (which are 0 for our ET_REL .tcm.elf) — applied by GDB as
       *     offs[SECT_OFF_TEXT] = text_addr;
       *     offs[SECT_OFF_DATA] = data_addr;
       *     offs[SECT_OFF_BSS]  = data_addr;     ← Bss value IGNORED
       *     objfile_relocate (...);
       * (gdb/remote.c, get_offsets(); gdb 7.2 forward and unchanged since —
       * Debian sources gdb/8.2.1 or sourceware.org legacy-ml/gdb-patches
       * 2007-05/msg00140.html, "ELF segment based qOffsets".)
       *
       * Behaviors verified against that source and this exact GDB build:
       *  1. gdb.texinfo: "while a Bss offset may be included in the
       *     response, GDB ignores this and instead applies the Data offset
       *     to the Bss section" — .bss honors Data_reported.
       *  2. The "Target reported unsupported offsets" warning fires iff
       *     bss_addr != data_addr (gdb/remote.c get_offsets) — harmless
       *     for us, but unavoidable while Bss != Data.
       *  3. ALL THREE fields are required: omitting Bss makes GDB reject
       *     the whole reply with "Malformed response to offset query"
       *     (user-observed).
       *
       * GDB dialect for an ET_REL symfile (maint info sections): the
       * non-text buckets additionally shift by the .text footprint
       * (textSize = dataBase - textBase) — the simulated data-group base
       * sits right after .text.  So the stub reports:
       *   Text = textBase
       *   Data = dataBase - textSize     (GDB lands .data on dataBase)
       *   Bss  = bssBase  - textSize     (GDB lands .bss on bssBase) */
      uint32_t text, data, bss;
      if (memory_get_image_info(0, &text, &data, &bss)) {
        uint32_t text_size = data - text; /* flat-layout .text footprint */
        char buf[96];
        snprintf(buf, sizeof buf, "Text=%x;Data=%x;Bss=%x", text,
                 data - text_size, bss - text_size);
        rsp_send_str(fd, buf);
      } else {
        rsp_send_str(fd, ""); /* layout unknown — let GDB keep its defaults */
      }
      return;
    }
    if (strncmp(p, "qThreadInfo", 11) == 0) {
      rsp_send_str(fd, "m1");
      return;
    }
    if (strncmp(p, "qThreadExtraInfo", 16) == 0) {
      rsp_send_str(fd, "TCMIPS");
      return;
    }
    if (strncmp(p, "qProcessInfo", 12) == 0) {
      rsp_send_str(fd, "pid:1;name:TCMIPSEmulator;");
      return;
    }
    rsp_send_str(fd, "");
    return;
  }

  case 'H':
    rsp_send_str(fd, "OK");
    return;
  case 'T':
    rsp_send_str(fd, "OK");
    return;
  case 'D': /* detach: leave the guest running */
    rsp_continue();
    rsp_send_str(fd, "OK");
    return;
  case 'k': /* kill: shut the guest down (no reply; teardown follows) */
    host_stop_running();
    return;

  default:
    rsp_send_str(fd, "");
    return;
  }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Server loop                                                          */
/* ════════════════════════════════════════════════════════════════════ */

static void rsp_server_thread(void *arg) {
  (void)arg;
  while (!g_stub_shutdown) {
    if (g_client_fd == SOCK_INVALID) {
      struct sockaddr_in client;
      socklen_t clen = sizeof(client);
      sock_t c = accept(g_listen_fd, (struct sockaddr *)&client, &clen);
      if (c == SOCK_INVALID)
        continue;
      g_client_fd = c;
      /* Request the CPU to pause so the debugger starts stopped. */
      rsp_interrupt();
      continue;
    }

    char packet[RSP_BUF_MAX];
    int ctrl = 0;
    int r = rsp_read_packet(g_client_fd, packet, sizeof(packet), &ctrl);
    if (r == 0) {
      if (ctrl == 0x03) {
        rsp_interrupt();
        rsp_send_str(g_client_fd, "S05");
      }
      continue;
    }
    if (r == 1) {
      uint8_t pcs = rsp_checksum(packet, strlen(packet));
      /* verify the checksum against the '#' digits already validated in read */
      (void)pcs;
      send(g_client_fd, "+", 1, 0);
      rsp_handle(packet, g_client_fd);
      continue;
    }
    /* -1: connection closed */
    SOCK_CLOSE(g_client_fd);
    g_client_fd = SOCK_INVALID;
  }

  if (g_client_fd != SOCK_INVALID) {
    SOCK_CLOSE(g_client_fd);
    g_client_fd = SOCK_INVALID;
  }
}

/* ════════════════════════════════════════════════════════════════════ */
/*  Public API                                                           */
/* ════════════════════════════════════════════════════════════════════ */

int rsp_stub_start(int port) {
  if (port == 0)
    port = RSP_DEFAULT_PORT;
  g_stub_shutdown = false;

#if defined(_WIN32) || defined(_WIN64)
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    return -1;
  g_wsa_inited = true;
#endif

  g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_listen_fd == SOCK_INVALID)
    return -1;

  int one = 1;
  setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one,
             sizeof(one));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)port);

  if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    SOCK_CLOSE(g_listen_fd);
    g_listen_fd = SOCK_INVALID;
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
    g_wsa_inited = false;
#endif
    return -2;
  }
  if (listen(g_listen_fd, 4) < 0) {
    SOCK_CLOSE(g_listen_fd);
    g_listen_fd = SOCK_INVALID;
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
    g_wsa_inited = false;
#endif
    return -3;
  }

  if (!platform_thread_create(&g_stub_thread, rsp_server_thread, NULL)) {
    SOCK_CLOSE(g_listen_fd);
    g_listen_fd = SOCK_INVALID;
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
    g_wsa_inited = false;
#endif
    return -4;
  }
  g_thread_started = true;
  return 0;
}

void rsp_stub_stop(void) {
  g_stub_shutdown = true;

  /* Close fds to unblock accept/recv. */
  if (g_listen_fd != SOCK_INVALID) {
    SOCK_CLOSE(g_listen_fd);
    g_listen_fd = SOCK_INVALID;
  }
  if (g_client_fd != SOCK_INVALID) {
    SOCK_CLOSE(g_client_fd);
    g_client_fd = SOCK_INVALID;
  }

  if (g_thread_started) {
    platform_thread_join(&g_stub_thread);
    g_thread_started = false;
  }

#if defined(_WIN32) || defined(_WIN64)
  if (g_wsa_inited) {
    WSACleanup();
    g_wsa_inited = false;
  }
#endif
}