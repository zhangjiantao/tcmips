/** Debug CPU Interpreter — Implementation
 *
 * Runs the guest one instruction at a time through the local
 * tcm_exec_instruction() dispatcher on the CPU thread, while a background
 * RSP stub thread drives execution: pausing on attach / interrupt, stepping,
 * continuing, and servicing software breakpoints modelled as a table of
 * addresses (the guest memory image is never patched, so memory reads always
 * show the original instruction).
 *
 * Execution is controlled by the following atomic variables shared with the
 * RSP stub thread:
 *   g_dbg_pause_requested — when true the CPU runs; when false the CPU is
 *                           stopped and spins until the stub resumes it.
 *   g_dbg_single_step     — when true the CPU executes exactly one instruction,
 *                           then stops again (single step).
 *   g_dbg_stop_event      — set by the CPU when it halts on a stop condition.
 *   g_dbg_exit_phase      — 0 = running, 1 = guest done, 2 = exit reply sent;
 *                           used to tear the stub down cleanly.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu_debug.h"
#include "host.h"
#include "memory.h"
#include "peripheral.h"
#include "platform.h"
#include "rsp_stub.h"

/* ── Shared CPU state & control flags ───────────────────────────── */

MIPS_CPU g_debug_cpu;

/* Atomic globals are zero-initialized (false) at load time; the values are
 * (re)set explicitly by cpu_debug_execute() before each run. */
atomic_bool g_dbg_pause_requested; /* gate: true = run, false = stop */
atomic_bool g_dbg_single_step;     /* run exactly one instruction */
atomic_bool g_dbg_stop_event;      /* CPU sets it the moment it halts */
atomic_uint g_dbg_exit_phase;      /* 0 run, 1 guest done, 2 W sent  */

/* ── Guest exit code ──────────────────────────── */
/* Storage lives here so the header-only execution core can call the
 * setters from inline handlers without instantiating per-TU state. */

static uint32_t cpu_debug_exit_code;

static void cpu_debug_set_exit_code(uint32_t v) { cpu_debug_exit_code = v; }

static void cpu_debug_exit_code_reset(void) { cpu_debug_exit_code = 0; }

uint32_t cpu_debug_get_exit_code(void) { return cpu_debug_exit_code; }

static uint64_t cpu_debug_get_count(void) {
  return 0; /* the debug interpreter tracks no instruction counter yet */
}

static void cpu_debug_reset_count(void) {}

static const char *cpu_debug_get_status(void) {
  if (!host_is_running())
    return "STOPPED";
  bool running =
      atomic_load_explicit(&g_dbg_pause_requested, memory_order_relaxed);
  return running ? "RUNNING" : "PAUSED";
}

/* Guest BREAK trap flag: set by cpu_debug_notify_guest_break() and consumed
 * by the main loop below.  Producer and consumer both run on the CPU
 * thread (the r_break handler executes in-line), so a plain bool is fine. */
static bool g_dbg_break_pending;

static void cpu_debug_notify_guest_break(uint32_t code, uint32_t pc) {
  fprintf(stderr,
          "cpu_debug: guest BREAK code=0x%03x at pc=0x%08x -> stop for "
          "debugger\n",
          code, pc);
  fflush(stderr);
  g_dbg_break_pending = true;
}

/** Apply a taken branch past its delay slot (MIPS delay-slot semantics).
 *  Once the delay-slot instruction has retired, load the branch target into
 *  PC and clear the pending state.
 *
 *  MUST run before any "what executes next" observation (debugger
 *  breakpoint checks, the single-step dispatch) so the branch target is
 *  visible: it only lands in PC after the delay slot finishes. */
static inline void tcm_resolve_pending_branch(MIPS_CPU *cpu) {
  if (cpu->pending_branch && !cpu->branch_delay_slot) {
    cpu->PC = cpu->pending_branch_target;
    cpu->pending_branch = false;
  }
}

/* ── Breakpoint table (shared with the RSP stub) ─────────────────── */

#define DBG_BP_MAX 256

typedef struct {
  uint32_t addr;
} dbg_bp_t;

/* Compact address table: live entries are kept in [0, used) so the
 * every-instruction hit test scans only live breakpoints (typically a
 * handful), and is O(1) with zero breakpoints — the common case while
 * stepping without any set.  Removal swaps against the tail.  No patch
 * model is used: guest memory is never modified for a breakpoint. */
static dbg_bp_t g_dbg_bps[DBG_BP_MAX];
static int g_dbg_bp_used;

int cpu_debug_set_breakpoint(uint32_t addr) {
  for (int i = 0; i < g_dbg_bp_used; i++) {
    if (g_dbg_bps[i].addr == addr)
      return 0; /* already set — idempotent */
  }
  if (g_dbg_bp_used >= DBG_BP_MAX)
    return -1;
  g_dbg_bps[g_dbg_bp_used].addr = addr;
  g_dbg_bp_used++;
  return 0;
}

int cpu_debug_remove_breakpoint(uint32_t addr) {
  for (int i = 0; i < g_dbg_bp_used; i++) {
    if (g_dbg_bps[i].addr == addr) {
      g_dbg_bp_used--;
      g_dbg_bps[i] = g_dbg_bps[g_dbg_bp_used]; /* swap with tail */
      return 0;
    }
  }
  return -1;
}

/* Hit test for the software breakpoint table, evaluated against the
 * instruction the guest is about to execute.
 *
 * Evaluating "what runs next" requires folding a pending branch target
 * into PC first (MIPS delay-slot semantics: after the delay slot has
 * retired, the real next instruction is the branch target, not the
 * stale fall-through address).  tcm_resolve_pending_branch() does that
 * fold; encapsulating it here keeps the main loop's breakpoint check a
 * single plain "is the next instruction a breakpoint?" test. */
static bool dbg_bp_hits_next(MIPS_CPU *cpu) {
  tcm_resolve_pending_branch(cpu);
  for (int i = 0; i < g_dbg_bp_used; i++)
    if (g_dbg_bps[i].addr == cpu->PC)
      return true;
  return false;
}

/* ── Spin helpers (blocks CPU thread while stopped) ──────────────── */

static void dbg_wait_for_run(void) {
  while (!atomic_load_explicit(&g_dbg_pause_requested, memory_order_relaxed)) {
    /* stub has not yet allowed us to run — sleep briefly */
    platform_sleep_ms(1);
  }
}

/* Close the run-gate, mark the CPU stopped, and signal the stub that a fresh
 * stop has happened (g_dbg_stop_event), then block until the stub resumes us.
 * Called at every place the interpreter halts on behalf of the debugger. */
static void dbg_stop_and_wait(void) {
  atomic_store_explicit(&g_dbg_pause_requested, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_stop_event, true, memory_order_relaxed);
  dbg_wait_for_run();
}

/* ==================================================================== */
/*  Instruction semantics (DEBUG interpreter copy - includes the         */
/*  BREAK-trap hook and exit-code setter; the FAST copy lives in         */
/*  cpu_fast.c).                                                         */
/* ==================================================================== */

/* ==================================================================== */
/*  Load / Store (no delay slot)                                        */
/* ==================================================================== */

static inline uint32_t h_lb(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] =
      sign_extend_8(mem_read_8(cpu->GPR[rs] + sign_extend_imm(instr)));
  return pc + 4;
}

static inline uint32_t h_lh(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] =
      sign_extend_16(mem_read_16(cpu->GPR[rs] + sign_extend_imm(instr)));
  return pc + 4;
}

static inline uint32_t h_lw(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = mem_read_32(cpu->GPR[rs] + sign_extend_imm(instr));
  return pc + 4;
}

static inline uint32_t h_lbu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = mem_read_8(cpu->GPR[rs] + sign_extend_imm(instr));
  return pc + 4;
}

static inline uint32_t h_lhu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = mem_read_16(cpu->GPR[rs] + sign_extend_imm(instr));
  return pc + 4;
}

static inline uint32_t h_sb(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  mem_write_8(cpu->GPR[rs] + sign_extend_imm(instr), cpu->GPR[rt] & 0xFF);
  return pc + 4;
}

static inline uint32_t h_sh(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  mem_write_16(cpu->GPR[rs] + sign_extend_imm(instr), cpu->GPR[rt] & 0xFFFF);
  return pc + 4;
}

static inline uint32_t h_sw(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  mem_write_32(cpu->GPR[rs] + sign_extend_imm(instr), cpu->GPR[rt]);
  return pc + 4;
}

/* ==================================================================== */
/*  I-Type ALU (no delay slot)                                          */
/* ==================================================================== */

static inline uint32_t h_addiu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                               bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = cpu->GPR[rs] + (uint32_t)sign_extend_imm(instr);
  return pc + 4;
}

static inline uint32_t h_slti(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = ((int32_t)cpu->GPR[rs] < sign_extend_imm(instr)) ? 1 : 0;
  return pc + 4;
}

static inline uint32_t h_sltiu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                               bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] =
      ((uint32_t)cpu->GPR[rs] < (uint32_t)sign_extend_imm(instr)) ? 1 : 0;
  return pc + 4;
}

static inline uint32_t h_andi(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = cpu->GPR[rs] & extract_imm(instr);
  return pc + 4;
}

static inline uint32_t h_ori(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = cpu->GPR[rs] | extract_imm(instr);
  return pc + 4;
}

static inline uint32_t h_xori(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = cpu->GPR[rs] ^ extract_imm(instr);
  return pc + 4;
}

static inline uint32_t h_lui(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rt = extract_rt(instr);
  cpu->GPR[rt] = (uint32_t)extract_imm(instr) << 16;
  return pc + 4;
}

/* ==================================================================== */
/*  J-Type (delay slot)                                                  */
/* ==================================================================== */

static inline uint32_t h_j(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                           bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t target = ((pc + 4) & 0xF0000000) | (extract_target(instr) << 2);
  cpu->pending_branch = true;
  cpu->pending_branch_target = target;
  cpu->branch_delay_slot = true;
  return pc + 4; /* delay-slot address */
}

static inline uint32_t h_jal(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  cpu->GPR[31] = pc + 8;
  uint32_t target = ((pc + 4) & 0xF0000000) | (extract_target(instr) << 2);
  cpu->pending_branch = true;
  cpu->pending_branch_target = target;
  cpu->branch_delay_slot = true;
  return pc + 4; /* delay-slot address */
}

/* ==================================================================== */
/*  I-Type Branch (delay slot)                                           */
/* ==================================================================== */

static inline uint32_t h_beq(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  bool taken = (cpu->GPR[rs] == cpu->GPR[rt]);
  uint32_t target = pc + 4 + ((uint32_t)sign_extend_imm(instr) << 2);
  if (taken) {
    cpu->pending_branch = true;
    cpu->pending_branch_target = target;
  }
  cpu->branch_delay_slot = true;
  return pc + 4;
}

static inline uint32_t h_bne(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  bool taken = (cpu->GPR[rs] != cpu->GPR[rt]);
  uint32_t target = pc + 4 + ((uint32_t)sign_extend_imm(instr) << 2);
  if (taken) {
    cpu->pending_branch = true;
    cpu->pending_branch_target = target;
  }
  cpu->branch_delay_slot = true;
  return pc + 4;
}

static inline uint32_t h_blez(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rs = extract_rs(instr);
  bool taken = ((int32_t)cpu->GPR[rs] <= 0);
  uint32_t target = pc + 4 + ((uint32_t)sign_extend_imm(instr) << 2);
  if (taken) {
    cpu->pending_branch = true;
    cpu->pending_branch_target = target;
  }
  cpu->branch_delay_slot = true;
  return pc + 4;
}

static inline uint32_t h_bgtz(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rs = extract_rs(instr);
  bool taken = ((int32_t)cpu->GPR[rs] > 0);
  uint32_t target = pc + 4 + ((uint32_t)sign_extend_imm(instr) << 2);
  if (taken) {
    cpu->pending_branch = true;
    cpu->pending_branch_target = target;
  }
  cpu->branch_delay_slot = true;
  return pc + 4;
}

/* ==================================================================== */
/*  REGIMM (delay slot)                                                  */
/* ==================================================================== */

static inline uint32_t h_regimm(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  bool taken = 0;
  switch ((RegImmRt)rt) {
  case RT_BLTZ:
    taken = ((int32_t)cpu->GPR[rs] < 0);
    break;
  case RT_BGEZ:
    taken = ((int32_t)cpu->GPR[rs] >= 0);
    break;
  default:
    return pc + 4; /* unknown REGIMM = NOP, no delay slot */
  }
  uint32_t target = pc + 4 + ((uint32_t)sign_extend_imm(instr) << 2);
  if (taken) {
    cpu->pending_branch = true;
    cpu->pending_branch_target = target;
  }
  cpu->branch_delay_slot = true;
  return pc + 4;
}

/* ==================================================================== */
/*  SPECIAL2 (no delay slot)                                            */
/* ==================================================================== */

static inline uint32_t h_special2(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                  bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint32_t rd = extract_rd(instr);
  uint32_t funct = extract_funct(instr);
  if ((Special2Funct)funct == FUN2_MUL) {
    int64_t res =
        (int64_t)(int32_t)cpu->GPR[rs] * (int64_t)(int32_t)cpu->GPR[rt];
    cpu->GPR[rd] = (uint32_t)(res & 0xFFFFFFFF);
  }
  return pc + 4;
}

/* ==================================================================== */
/*  SPECIAL3 (no delay slot)                                            */
/* ==================================================================== */

static inline uint32_t h_special3(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                  bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rt = extract_rt(instr);
  uint32_t rd = extract_rd(instr);
  uint32_t sa = extract_sa(instr);
  uint32_t funct = extract_funct(instr);
  if ((Special3Funct)funct == FUN3_BEXT) {
    switch ((Special3Sa)sa) {
    case SA_WSBH:
      cpu->GPR[rd] = ((cpu->GPR[rt] >> 8) & 0x00FF00FF) |
                     ((cpu->GPR[rt] << 8) & 0xFF00FF00);
      break;
    case SA_SEB:
      cpu->GPR[rd] = sign_extend_8(cpu->GPR[rt] & 0xFF);
      break;
    case SA_SEH:
      cpu->GPR[rd] = sign_extend_16(cpu->GPR[rt] & 0xFFFF);
      break;
    }
  }
  return pc + 4;
}

/* ==================================================================== */
/*  R-Type (SPECIAL) handlers                                             */
/* ==================================================================== */

/* R-Type delay-slot branches */

static inline uint32_t r_jr(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t target = cpu->GPR[extract_rs(instr)];
  cpu->pending_branch = true;
  cpu->pending_branch_target = target;
  cpu->branch_delay_slot = true;
  return pc + 4; /* delay-slot address */
}

static inline uint32_t r_jalr(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  if (in_delay_slot)
    return pc + 4;
  uint32_t rd = extract_rd(instr);
  uint32_t target = cpu->GPR[extract_rs(instr)];
  cpu->GPR[rd] = pc + 8;
  cpu->pending_branch = true;
  cpu->pending_branch_target = target;
  cpu->branch_delay_slot = true;
  return pc + 4; /* delay-slot address */
}

/* R-Type shifts */

static inline uint32_t r_sll(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = cpu->GPR[extract_rt(instr)]
                                << extract_sa(instr);
  return pc + 4;
}

static inline uint32_t r_srl(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint32_t sa = extract_sa(instr);
  if (rs == 0)
    cpu->GPR[extract_rd(instr)] = cpu->GPR[rt] >> sa;
  else if (rs == 1) {
    /* ROTR */
    uint32_t u32reg = cpu->GPR[rt] >> sa;
    cpu->GPR[extract_rd(instr)] = (cpu->GPR[rt] << (32 - sa)) | u32reg;
  }
  return pc + 4;
}

static inline uint32_t r_sra(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      (uint32_t)((int32_t)cpu->GPR[extract_rt(instr)] >> extract_sa(instr));
  return pc + 4;
}

static inline uint32_t r_sllv(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = cpu->GPR[extract_rt(instr)]
                                << (cpu->GPR[extract_rs(instr)] & 0x1F);
  return pc + 4;
}

static inline uint32_t r_srlv(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t sa = extract_sa(instr);
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint32_t rd = extract_rd(instr);
  if (sa == 0)
    cpu->GPR[rd] = cpu->GPR[rt] >> (cpu->GPR[rs] & 0x1F);
  else if (sa == 1) {
    /* ROTRV */
    uint32_t shift = cpu->GPR[rs] & 0x1F;
    uint32_t u32reg = cpu->GPR[rt] >> shift;
    cpu->GPR[rd] = (cpu->GPR[rt] << (32 - shift)) | u32reg;
  }
  return pc + 4;
}

static inline uint32_t r_srav(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      (uint32_t)((int32_t)cpu->GPR[extract_rt(instr)] >>
                 (cpu->GPR[extract_rs(instr)] & 0x1F));
  return pc + 4;
}

/* R-Type moves and traps */

static inline uint32_t r_movz(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint32_t rd = extract_rd(instr);
  if (cpu->GPR[rt] == 0)
    cpu->GPR[rd] = cpu->GPR[rs];
  return pc + 4;
}

static inline uint32_t r_movn(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint32_t rd = extract_rd(instr);
  if (cpu->GPR[rt] != 0)
    cpu->GPR[rd] = cpu->GPR[rs];
  return pc + 4;
}

static inline uint32_t r_syscall(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                 bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t sa = extract_sa(instr);
  uint32_t rd = extract_rd(instr);
  cpu->GPR[2] = syscall(sa, rd, cpu->GPR[4], cpu->GPR[5]);
  return pc + 4;
}

static inline uint32_t r_break(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                               bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t code = extract_break_code(instr);
  uint32_t reason = cpu->GPR[4];

  if (code == 0x3ff && host_is_running()) {
    /* BREAK(code==0x3ff) — guest has explicitly called exit().
     * reason == 0  →  normal exit, exit code is in GPR[5].
     * reason != 0  →  abort / unexpected terminate.
     * In both cases we capture GPR[5] as the exit code. */
    cpu_debug_set_exit_code(cpu->GPR[5]);

    if (reason == 0) {
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR EXIT, code=%u !!\n",
              cpu_debug_get_exit_code());
    } else {
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR ABORT, reason=%u, code=%u !!\n",
              reason, cpu_debug_get_exit_code());
    }
    host_stop_running();
    return pc + 4;
  }

  /* Non-exit break codes (libc assert, __builtin_trap, raw BREAK ...):
   * inside a debug session the guest is stopped and the debugger receives a
   * SIGTRAP stop reply (PC has already advanced past the BREAK, so a plain
   * 'continue' makes progress).  The fast interpreter simply skips the
   * instruction (return pc + 4) because it has no debugger to notify. */
  cpu_debug_notify_guest_break(code, pc + 4);

  return pc + 4;
}

/* R-Type HI/LO */

static inline uint32_t r_mfhi(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = cpu->HI;
  return pc + 4;
}

static inline uint32_t r_mthi(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->HI = cpu->GPR[extract_rs(instr)];
  return pc + 4;
}

static inline uint32_t r_mflo(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = cpu->LO;
  return pc + 4;
}

static inline uint32_t r_mtlo(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->LO = cpu->GPR[extract_rs(instr)];
  return pc + 4;
}

/* R-Type mult / div */

static inline uint32_t r_mult(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  int64_t res = (int64_t)(int32_t)cpu->GPR[rs] * (int64_t)(int32_t)cpu->GPR[rt];
  cpu->LO = (uint32_t)(res & 0xFFFFFFFF);
  cpu->HI = (uint32_t)((res >> 32) & 0xFFFFFFFF);
  return pc + 4;
}

static inline uint32_t r_multu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                               bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  uint64_t res = (uint64_t)cpu->GPR[rs] * (uint64_t)cpu->GPR[rt];
  cpu->LO = (uint32_t)(res & 0xFFFFFFFF);
  cpu->HI = (uint32_t)((res >> 32) & 0xFFFFFFFF);
  return pc + 4;
}

static inline uint32_t r_div(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  if (cpu->GPR[rt] != 0) {
    cpu->LO = (uint32_t)((int32_t)cpu->GPR[rs] / (int32_t)cpu->GPR[rt]);
    cpu->HI = (uint32_t)((int32_t)cpu->GPR[rs] % (int32_t)cpu->GPR[rt]);
  }
  return pc + 4;
}

static inline uint32_t r_divu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  uint32_t rs = extract_rs(instr);
  uint32_t rt = extract_rt(instr);
  if (cpu->GPR[rt] != 0) {
    cpu->LO = cpu->GPR[rs] / cpu->GPR[rt];
    cpu->HI = cpu->GPR[rs] % cpu->GPR[rt];
  }
  return pc + 4;
}

/* R-Type ALU */

static inline uint32_t r_addu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      cpu->GPR[extract_rs(instr)] + cpu->GPR[extract_rt(instr)];
  return pc + 4;
}

static inline uint32_t r_subu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      cpu->GPR[extract_rs(instr)] - cpu->GPR[extract_rt(instr)];
  return pc + 4;
}

static inline uint32_t r_and(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      cpu->GPR[extract_rs(instr)] & cpu->GPR[extract_rt(instr)];
  return pc + 4;
}

static inline uint32_t r_or(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                            bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      cpu->GPR[extract_rs(instr)] | cpu->GPR[extract_rt(instr)];
  return pc + 4;
}

static inline uint32_t r_xor(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      cpu->GPR[extract_rs(instr)] ^ cpu->GPR[extract_rt(instr)];
  return pc + 4;
}

static inline uint32_t r_nor(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] =
      ~(cpu->GPR[extract_rs(instr)] | cpu->GPR[extract_rt(instr)]);
  return pc + 4;
}

static inline uint32_t r_slt(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                             bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = ((int32_t)cpu->GPR[extract_rs(instr)] <
                                 (int32_t)cpu->GPR[extract_rt(instr)])
                                    ? 1
                                    : 0;
  return pc + 4;
}

static inline uint32_t r_sltu(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                              bool in_delay_slot) {
  (void)in_delay_slot;
  cpu->GPR[extract_rd(instr)] = ((uint32_t)cpu->GPR[extract_rs(instr)] <
                                 (uint32_t)cpu->GPR[extract_rt(instr)])
                                    ? 1
                                    : 0;
  return pc + 4;
}

/* Fetch & execute the instruction at cpu->PC, applying the pending-branch
 * rule so cpu->PC is left at the address of the NEXT instruction to
 * dispatch.  Inlined into the debug single-step loop — no cross-TU call
 * stands between the hot loop and the decoder. */

/* ==================================================================== */
/*  Dispatch: SPECIAL subgroup + top level (switch, no jump tables)      */
/* ==================================================================== */

static inline uint32_t h_special(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                 bool in_delay_slot) {
  switch (extract_funct(instr)) {
  case FUN_SLL:
    return r_sll(cpu, instr, pc, in_delay_slot);
  case FUN_SRL:
    return r_srl(cpu, instr, pc, in_delay_slot);
  case FUN_SRA:
    return r_sra(cpu, instr, pc, in_delay_slot);
  case FUN_SLLV:
    return r_sllv(cpu, instr, pc, in_delay_slot);
  case FUN_SRLV:
    return r_srlv(cpu, instr, pc, in_delay_slot);
  case FUN_SRAV:
    return r_srav(cpu, instr, pc, in_delay_slot);
  case FUN_JR:
    return r_jr(cpu, instr, pc, in_delay_slot);
  case FUN_JALR:
    return r_jalr(cpu, instr, pc, in_delay_slot);
  case FUN_MOVZ:
    return r_movz(cpu, instr, pc, in_delay_slot);
  case FUN_MOVN:
    return r_movn(cpu, instr, pc, in_delay_slot);
  case FUN_SYSCALL:
    return r_syscall(cpu, instr, pc, in_delay_slot);
  case FUN_BREAK:
    return r_break(cpu, instr, pc, in_delay_slot);
  case FUN_MFHI:
    return r_mfhi(cpu, instr, pc, in_delay_slot);
  case FUN_MTHI:
    return r_mthi(cpu, instr, pc, in_delay_slot);
  case FUN_MFLO:
    return r_mflo(cpu, instr, pc, in_delay_slot);
  case FUN_MTLO:
    return r_mtlo(cpu, instr, pc, in_delay_slot);
  case FUN_MULT:
    return r_mult(cpu, instr, pc, in_delay_slot);
  case FUN_MULTU:
    return r_multu(cpu, instr, pc, in_delay_slot);
  case FUN_DIV:
    return r_div(cpu, instr, pc, in_delay_slot);
  case FUN_DIVU:
    return r_divu(cpu, instr, pc, in_delay_slot);
  case FUN_ADDU:
    return r_addu(cpu, instr, pc, in_delay_slot);
  case FUN_SUBU:
    return r_subu(cpu, instr, pc, in_delay_slot);
  case FUN_AND:
    return r_and(cpu, instr, pc, in_delay_slot);
  case FUN_OR:
    return r_or(cpu, instr, pc, in_delay_slot);
  case FUN_XOR:
    return r_xor(cpu, instr, pc, in_delay_slot);
  case FUN_NOR:
    return r_nor(cpu, instr, pc, in_delay_slot);
  case FUN_SLT:
    return r_slt(cpu, instr, pc, in_delay_slot);
  case FUN_SLTU:
    return r_sltu(cpu, instr, pc, in_delay_slot);
  default:
    return pc + 4; /* unimplemented SPECIAL funct = NOP */
  }
}

static inline uint32_t tcm_exec_instruction(MIPS_CPU *cpu) {
  /* Apply pending-branch target only once the delay-slot instruction has
   * finished (also available to observers via tcm_resolve_pending_branch). */
  tcm_resolve_pending_branch(cpu);

  uint32_t instr = mem_read_32(cpu->PC);
  bool in_delay_slot = cpu->branch_delay_slot;
  cpu->branch_delay_slot = false;
  switch ((instr >> 26) & 0x3F) {
  case OP_SPECIAL:
    cpu->PC = h_special(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_REGIMM:
    cpu->PC = h_regimm(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_J:
    cpu->PC = h_j(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_JAL:
    cpu->PC = h_jal(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_BEQ:
    cpu->PC = h_beq(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_BNE:
    cpu->PC = h_bne(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_BLEZ:
    cpu->PC = h_blez(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_BGTZ:
    cpu->PC = h_bgtz(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_ADDIU:
    cpu->PC = h_addiu(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SLTI:
    cpu->PC = h_slti(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SLTIU:
    cpu->PC = h_sltiu(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_ANDI:
    cpu->PC = h_andi(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_ORI:
    cpu->PC = h_ori(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_XORI:
    cpu->PC = h_xori(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LUI:
    cpu->PC = h_lui(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SPECIAL2:
    cpu->PC = h_special2(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SPECIAL3:
    cpu->PC = h_special3(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LB:
    cpu->PC = h_lb(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LH:
    cpu->PC = h_lh(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LW:
    cpu->PC = h_lw(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LBU:
    cpu->PC = h_lbu(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_LHU:
    cpu->PC = h_lhu(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SB:
    cpu->PC = h_sb(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SH:
    cpu->PC = h_sh(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  case OP_SW:
    cpu->PC = h_sw(cpu, instr, cpu->PC, in_delay_slot);
    return cpu->PC;
  default:
    return cpu->PC += 4;
  }
}

/* ── Main execution loop ─────────────────────────────────────────── */

void cpu_debug_execute(void) {
  memset(&g_debug_cpu, 0, sizeof(MIPS_CPU));
  g_debug_cpu.GPR[0] = 0;

  atomic_store_explicit(&g_dbg_pause_requested, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_single_step, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_stop_event, false, memory_order_relaxed);
  atomic_store_explicit(&g_dbg_exit_phase, 0, memory_order_relaxed);
  g_dbg_break_pending = false;

  cpu_debug_exit_code_reset();

  /* Start the RSP stub thread (background). */
  if (rsp_stub_start(1234) != 0) {
    fprintf(stderr, "cpu_debug: failed to start RSP stub\n");
    return;
  }
  /* The stub pauses the CPU on attach (g_dbg_pause_requested = false). */
  while (host_is_running()) {
    /* Run-gate: the CPU runs only while the gate is open (true).  While it is
     * closed (interrupt requested / before the debugger continues), block. */
    if (!atomic_load_explicit(&g_dbg_pause_requested, memory_order_relaxed)) {
      dbg_stop_and_wait();
      continue;
    }

    /* Software breakpoints are modelled as an address table — the guest image
     * is never patched, so memory reads always expose the original
     * instruction.  Stop BEFORE executing the instruction at an active
     * breakpoint address, then when the debugger resumes (either 'c' or 's')
     * execute exactly that one instruction: this is the classic "step over"
     * which lets a plain continue make progress past a breakpoint instead of
     * hitting it again immediately. */
    bool stepped_over = false;
    if (dbg_bp_hits_next(&g_debug_cpu)) {
      dbg_stop_and_wait();
      if (!host_is_running())
        break;
      tcm_exec_instruction(&g_debug_cpu);
      stepped_over = true;
    }

    if (!stepped_over)
      tcm_exec_instruction(&g_debug_cpu);

    /* If the guest terminated (BREAK(0x3ff)), the host flag is cleared by the
     * shared break handler; the outer loop exits. */
    if (!host_is_running())
      break;

    /* The guest executed a BREAK with a non-exit code (assert/trap/...):
     * hand it to the debugger as SIGTRAP.  PC has already advanced past the
     * BREAK, so a plain 'continue' makes progress instead of re-executing
     * it in a loop.  Checked before the single-step gate so a 's' that ran
     * into a BREAK still stops here (the single-step flag is honoured on
     * the next resume). */
    if (g_dbg_break_pending) {
      g_dbg_break_pending = false;
      dbg_stop_and_wait();
      continue;
    }

    /* Single-step gating: after exactly one instruction following a 's'
     * command (whether it was a step-over or a normal step), stop and wait. */
    if (atomic_exchange_explicit(&g_dbg_single_step, false,
                                 memory_order_relaxed)) {
      dbg_stop_and_wait();
    }
  }

  /* Guest finished: flag it before tearing the stub down so the RSP stub
   * can deliver a proper process-exit stop reply ('W<exit-code>').  Wait
   * (bounded) for the stub to confirm the packet was sent, otherwise the
   * socket close in rsp_stub_stop() would race with the reply. */
  atomic_store_explicit(&g_dbg_exit_phase, 1, memory_order_relaxed);
  for (int i = 0; i < 150 && atomic_load_explicit(&g_dbg_exit_phase,
                                                  memory_order_relaxed) < 2;
       i++)
    platform_sleep_ms(2);

  rsp_stub_stop();
}

const cpu_interpreter_t g_interp_debug = {
    .name = "DEBUG",
    .get_status = cpu_debug_get_status,
    .execute = cpu_debug_execute,
    .get_count = cpu_debug_get_count,
    .reset_count = cpu_debug_reset_count,
    .get_exit_code = cpu_debug_get_exit_code,
};