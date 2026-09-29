/* * Fast CPU Interpreter
 *
 * Hot path of the TCMIPS emulator: a computed-goto dispatch loop.  The full
 * instruction semantics live in this file (r_break omits the debugger hook;
 * everything else is the shared semantic set).  Each dispatch label calls a
 * static-inline h_* r_* handler; the compiler inlines them at -O, so there
 * is no runtime call overhead between the decoder and the handler body.
 *
 * The debug interpreter (cpu_debug.c) drives the same semantics through a
 * single-step dispatch with BREAK-trap/exit hooks and a shared memory-model.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "host.h"
#include "memory.h"
#include "peripheral.h"

/* Shared instruction-counter (updated once per CHECK_INTERVAL batch). */
_Atomic uint64_t g_insn_count = 0;

/* FAST interpreter's own exit code (BREAK(0x3ff)); reported through the
 * cpu_interpreter_t vtable (cpu_fast_get_exit_code). */
static uint32_t cpu_fast_exit_code;

static void cpu_fast_set_exit_code(uint32_t v) { cpu_fast_exit_code = v; }

static void cpu_fast_exit_code_reset(void) { cpu_fast_exit_code = 0; }

static uint32_t cpu_fast_get_exit_code(void) { return cpu_fast_exit_code; }

static uint64_t cpu_fast_get_count(void) {
  return atomic_load_explicit(&g_insn_count, memory_order_relaxed);
}

static void cpu_fast_reset_count(void) {
  atomic_store_explicit(&g_insn_count, 0, memory_order_relaxed);
}

static const char *cpu_fast_get_status(void) {
  bool running = host_is_running();
  return running ? "RUNNING" : "STOPPED";
}

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
    /* BREAK(code==0x3ff) - guest has explicitly called exit().
     * reason == 0  ->  normal exit, exit code is in GPR[5].
     * reason != 0  ->  abort / unexpected terminate.
     * In both cases we capture GPR[5] as the exit code.
     * (FAST interpreter: no debugger attached, no BREAK-trap reporting -
     *  non-exit codes just fall through to the skip below.) */
    cpu_fast_set_exit_code(cpu->GPR[5]);

    if (reason == 0) {
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR EXIT, code=%u !!\n",
              cpu_fast_get_exit_code());
    } else {
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR ABORT, reason=%u, code=%u !!\n",
              reason, cpu_fast_get_exit_code());
    }
    host_stop_running();
    return pc + 4;
  }

  /* Non-exit break codes (libc assert, __builtin_trap, raw BREAK ...):
   * FAST interpreter has no debugger - the skip is kept. */
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

/* Portable top-level dispatch (MSVC fallback path only): same semantics
 * as the computed-goto labels, going through the h_* handlers. */
static inline uint32_t cpu_fast_portable_dispatch(MIPS_CPU *cpu, uint32_t instr,
                                                  uint32_t pc,
                                                  bool in_delay_slot) {
  switch ((instr >> 26) & 0x3F) {
  case OP_SPECIAL:
    return h_special(cpu, instr, pc, in_delay_slot);
  case OP_REGIMM:
    return h_regimm(cpu, instr, pc, in_delay_slot);
  case OP_J:
    return h_j(cpu, instr, pc, in_delay_slot);
  case OP_JAL:
    return h_jal(cpu, instr, pc, in_delay_slot);
  case OP_BEQ:
    return h_beq(cpu, instr, pc, in_delay_slot);
  case OP_BNE:
    return h_bne(cpu, instr, pc, in_delay_slot);
  case OP_BLEZ:
    return h_blez(cpu, instr, pc, in_delay_slot);
  case OP_BGTZ:
    return h_bgtz(cpu, instr, pc, in_delay_slot);
  case OP_ADDIU:
    return h_addiu(cpu, instr, pc, in_delay_slot);
  case OP_SLTI:
    return h_slti(cpu, instr, pc, in_delay_slot);
  case OP_SLTIU:
    return h_sltiu(cpu, instr, pc, in_delay_slot);
  case OP_ANDI:
    return h_andi(cpu, instr, pc, in_delay_slot);
  case OP_ORI:
    return h_ori(cpu, instr, pc, in_delay_slot);
  case OP_XORI:
    return h_xori(cpu, instr, pc, in_delay_slot);
  case OP_LUI:
    return h_lui(cpu, instr, pc, in_delay_slot);
  case OP_SPECIAL2:
    return h_special2(cpu, instr, pc, in_delay_slot);
  case OP_SPECIAL3:
    return h_special3(cpu, instr, pc, in_delay_slot);
  case OP_LB:
    return h_lb(cpu, instr, pc, in_delay_slot);
  case OP_LH:
    return h_lh(cpu, instr, pc, in_delay_slot);
  case OP_LW:
    return h_lw(cpu, instr, pc, in_delay_slot);
  case OP_LBU:
    return h_lbu(cpu, instr, pc, in_delay_slot);
  case OP_LHU:
    return h_lhu(cpu, instr, pc, in_delay_slot);
  case OP_SB:
    return h_sb(cpu, instr, pc, in_delay_slot);
  case OP_SH:
    return h_sh(cpu, instr, pc, in_delay_slot);
  case OP_SW:
    return h_sw(cpu, instr, pc, in_delay_slot);
  default:
    return pc + 4;
  }
}

static inline uint32_t h_reserved(MIPS_CPU *cpu, uint32_t instr, uint32_t pc,
                                  bool in_delay_slot) {
  (void)cpu;
  (void)instr;
  (void)in_delay_slot;
  return pc + 4;
}

/* OPCODE-list for the computed-goto dispatch: X(opcode_const, label,
 * handler_fn).  Handlers are the local static ones above. */

#if defined(__APPLE__) && defined(__arm64__)

#define OPCODE_LIST(X)                                                         \
  X(OP_REGIMM, regimm, h_regimm)                                               \
  X(OP_J, j, h_j)                                                              \
  X(OP_JAL, jal, h_jal)                                                        \
  X(OP_BEQ, beq, h_beq)                                                        \
  X(OP_BNE, bne, h_bne)                                                        \
  X(OP_BLEZ, blez, h_blez)                                                     \
  X(OP_BGTZ, bgtz, h_bgtz)                                                     \
  X(OP_ADDIU, addiu, h_addiu)                                                  \
  X(OP_SLTI, slti, h_slti)                                                     \
  X(OP_SLTIU, sltiu, h_sltiu)                                                  \
  X(OP_ANDI, andi, h_andi)                                                     \
  X(OP_ORI, ori, h_ori)                                                        \
  X(OP_XORI, xori, h_xori)                                                     \
  X(OP_LUI, lui, h_lui)                                                        \
  X(OP_SPECIAL2, special2, h_special2)                                         \
  X(OP_SPECIAL3, special3, h_special3)                                         \
  X(OP_LB, lb, h_lb)                                                           \
  X(OP_LH, lh, h_lh)                                                           \
  X(OP_LW, lw, h_lw)                                                           \
  X(OP_LBU, lbu, h_lbu)                                                        \
  X(OP_LHU, lhu, h_lhu)                                                        \
  X(OP_SB, sb, h_sb)                                                           \
  X(OP_SH, sh, h_sh)                                                           \
  X(OP_SW, sw, h_sw)

#else

#define OPCODE_LIST(X)                                                         \
  X(OP_REGIMM, regimm, h_regimm)                                               \
  X(OP_J, j, h_j)                                                              \
  X(OP_JAL, jal, h_jal)                                                        \
  X(OP_BEQ, beq, h_beq)                                                        \
  X(OP_BNE, bne, h_bne)                                                        \
  X(OP_BLEZ, blez, h_blez)                                                     \
  X(OP_BGTZ, bgtz, h_bgtz)                                                     \
  X(/*OP_RESERVED*/ 8, reserved_08, h_reserved)                                \
  X(OP_ADDIU, addiu, h_addiu)                                                  \
  X(OP_SLTI, slti, h_slti)                                                     \
  X(OP_SLTIU, sltiu, h_sltiu)                                                  \
  X(OP_ANDI, andi, h_andi)                                                     \
  X(OP_ORI, ori, h_ori)                                                        \
  X(OP_XORI, xori, h_xori)                                                     \
  X(OP_LUI, lui, h_lui)                                                        \
  X(/*OP_RESERVED*/ 16, reserved_16, h_reserved)                               \
  X(/*OP_RESERVED*/ 17, reserved_17, h_reserved)                               \
  X(/*OP_RESERVED*/ 18, reserved_18, h_reserved)                               \
  X(/*OP_RESERVED*/ 19, reserved_19, h_reserved)                               \
  X(/*OP_RESERVED*/ 20, reserved_20, h_reserved)                               \
  X(/*OP_RESERVED*/ 21, reserved_21, h_reserved)                               \
  X(/*OP_RESERVED*/ 22, reserved_22, h_reserved)                               \
  X(/*OP_RESERVED*/ 23, reserved_23, h_reserved)                               \
  X(/*OP_RESERVED*/ 24, reserved_24, h_reserved)                               \
  X(/*OP_RESERVED*/ 25, reserved_25, h_reserved)                               \
  X(/*OP_RESERVED*/ 26, reserved_26, h_reserved)                               \
  X(/*OP_RESERVED*/ 27, reserved_27, h_reserved)                               \
  X(OP_SPECIAL2, special2, h_special2)                                         \
  X(/*OP_RESERVED*/ 29, reserved_29, h_reserved)                               \
  X(/*OP_RESERVED*/ 30, reserved_30, h_reserved)                               \
  X(OP_SPECIAL3, special3, h_special3)                                         \
  X(OP_LB, lb, h_lb)                                                           \
  X(OP_LH, lh, h_lh)                                                           \
  X(/*OP_RESERVED*/ 34, reserved_34, h_reserved)                               \
  X(OP_LW, lw, h_lw)                                                           \
  X(OP_LBU, lbu, h_lbu)                                                        \
  X(OP_LHU, lhu, h_lhu)                                                        \
  X(/*OP_RESERVED*/ 38, reserved_38, h_reserved)                               \
  X(/*OP_RESERVED*/ 39, reserved_39, h_reserved)                               \
  X(/*OP_RESERVED*/ 40, reserved_40, h_reserved)                               \
  X(/*OP_RESERVED*/ 41, reserved_41, h_reserved)                               \
  X(OP_SB, sb, h_sb)                                                           \
  X(OP_SH, sh, h_sh)                                                           \
  X(/*OP_RESERVED*/ 44, reserved_44, h_reserved)                               \
  X(/*OP_RESERVED*/ 45, reserved_45, h_reserved)                               \
  X(/*OP_RESERVED*/ 47, reserved_47, h_reserved)                               \
  X(OP_SW, sw, h_sw)                                                           \
  X(/*OP_RESERVED*/ 48, reserved_48, h_reserved)                               \
  X(/*OP_RESERVED*/ 49, reserved_49, h_reserved)                               \
  X(/*OP_RESERVED*/ 50, reserved_50, h_reserved)                               \
  X(/*OP_RESERVED*/ 51, reserved_51, h_reserved)                               \
  X(/*OP_RESERVED*/ 52, reserved_52, h_reserved)                               \
  X(/*OP_RESERVED*/ 53, reserved_53, h_reserved)                               \
  X(/*OP_RESERVED*/ 54, reserved_54, h_reserved)                               \
  X(/*OP_RESERVED*/ 55, reserved_55, h_reserved)                               \
  X(/*OP_RESERVED*/ 56, reserved_56, h_reserved)                               \
  X(/*OP_RESERVED*/ 57, reserved_57, h_reserved)                               \
  X(/*OP_RESERVED*/ 58, reserved_58, h_reserved)                               \
  X(/*OP_RESERVED*/ 59, reserved_59, h_reserved)                               \
  X(/*OP_RESERVED*/ 60, reserved_60, h_reserved)                               \
  X(/*OP_RESERVED*/ 61, reserved_61, h_reserved)                               \
  X(/*OP_RESERVED*/ 62, reserved_62, h_reserved)                               \
  X(/*OP_RESERVED*/ 63, reserved_63, h_reserved)

#endif

/* SPECIAL-subgroup dispatch (second-level computed-goto):
 * X(funct_const, label, handler_fn).  The labels expand to
 * op_r_##label inside cpu_fast_execute(); r_labels[] falls back to
 * the shared op_skip (= NOP, matching h_special's default) for
 * unimplemented functs. */
#define RTYPE_LIST(X)                                                          \
  X(FUN_SLL, r_sll, r_sll)                                                     \
  X(FUN_SRL, r_srl, r_srl)                                                     \
  X(FUN_SRA, r_sra, r_sra)                                                     \
  X(FUN_SLLV, r_sllv, r_sllv)                                                  \
  X(FUN_SRLV, r_srlv, r_srlv)                                                  \
  X(FUN_SRAV, r_srav, r_srav)                                                  \
  X(FUN_JR, r_jr, r_jr)                                                        \
  X(FUN_JALR, r_jalr, r_jalr)                                                  \
  X(FUN_MOVZ, r_movz, r_movz)                                                  \
  X(FUN_MOVN, r_movn, r_movn)                                                  \
  X(FUN_SYSCALL, r_syscall, r_syscall)                                         \
  X(FUN_BREAK, r_break, r_break)                                               \
  X(FUN_MFHI, r_mfhi, r_mfhi)                                                  \
  X(FUN_MTHI, r_mthi, r_mthi)                                                  \
  X(FUN_MFLO, r_mflo, r_mflo)                                                  \
  X(FUN_MTLO, r_mtlo, r_mtlo)                                                  \
  X(FUN_MULT, r_mult, r_mult)                                                  \
  X(FUN_MULTU, r_multu, r_multu)                                               \
  X(FUN_DIV, r_div, r_div)                                                     \
  X(FUN_DIVU, r_divu, r_divu)                                                  \
  X(FUN_ADDU, r_addu, r_addu)                                                  \
  X(FUN_SUBU, r_subu, r_subu)                                                  \
  X(FUN_AND, r_and, r_and)                                                     \
  X(FUN_OR, r_or, r_or)                                                        \
  X(FUN_XOR, r_xor, r_xor)                                                     \
  X(FUN_NOR, r_nor, r_nor)                                                     \
  X(FUN_SLT, r_slt, r_slt)                                                     \
  X(FUN_SLTU, r_sltu, r_sltu)

/* Handler forward declarations (referenced both by the dispatch labels
 * below and by OPCODE_LIST itself). */
#define FWD_DECL_H(opcode_enum, label, func)                                   \
  static inline uint32_t func(MIPS_CPU *, uint32_t, uint32_t, bool);
OPCODE_LIST(FWD_DECL_H)

void cpu_fast_execute(void) {
  MIPS_CPU cpu;
  memset(&cpu, 0, sizeof(MIPS_CPU));

  /* Reset runtime bookkeeping so each new guest run starts at zero. */
  cpu_fast_reset_count();
  cpu_fast_exit_code_reset();

  /* $0 is hardwired zero */
  cpu.GPR[0] = 0;

  const uint32_t CHECK_INTERVAL = 0x3FFFFF; /* check running flag ~4M instr. */

  /* Computed-goto primary dispatch is only available on GCC and Clang.
     MSVC falls back to the portable function-pointer dispatch. */
#if ((defined(__GNUC__) || defined(__clang__)) && !defined(CPU_DEBUG_TRACE))
  {
    static void *top_labels[64];
    static void *r_labels[64]; /* SPECIAL-subgroup (funct) dispatch */
    static int labels_inited = 0;
    if (!labels_inited) {
      for (int i = 0; i < 64; i++) {
        top_labels[i] = &&op_skip;
        r_labels[i] = &&op_skip;
      }
      top_labels[OP_SPECIAL] = &&op_special;
#define SET_LABEL_ENTRY(opcode_enum, label, func)                              \
  top_labels[opcode_enum] = &&op_##label;
      OPCODE_LIST(SET_LABEL_ENTRY)
#define SET_LABEL_ENTRY_R(funct_const, label, func)                            \
  r_labels[funct_const] = &&op_##label;
      RTYPE_LIST(SET_LABEL_ENTRY_R)
      labels_inited = 1;
    }

    /* Local hot-path counter - avoids an expensive atomic RMW on every
       instruction.  Synced to the shared atomic g_insn_count at every
       CHECK_INTERVAL boundary (see DISPATCH). */
    uint64_t local_count = 0;

#define DISPATCH()                                                             \
  do {                                                                         \
    /* Apply a pending branch target only once the delay-slot instruction has  \
     * finished.  branch_delay_slot is set by a branch/jump handler to guard   \
     * exactly one instruction: the delay slot.                                \
     * After it executes, the following DISPATCH applies the target. */        \
    if (cpu.pending_branch && !cpu.branch_delay_slot) {                        \
      cpu.PC = cpu.pending_branch_target;                                      \
      cpu.pending_branch = false;                                              \
    }                                                                          \
    if ((local_count & CHECK_INTERVAL) == 0) {                                 \
      atomic_store_explicit(&g_insn_count, local_count, memory_order_relaxed); \
      if (!host_is_running())                                                  \
        goto emu_done;                                                         \
    }                                                                          \
    instr = mem_read_32(cpu.PC);                                               \
    goto *top_labels[(instr >> 26) & 0x3F];                                    \
  } while (0)

#define DEFINE_LABEL(opcode_enum, label, func)                                 \
  op_##label : {                                                               \
    bool _is_delay_slot = cpu.branch_delay_slot;                               \
    cpu.branch_delay_slot = false;                                             \
    cpu.PC = func(&cpu, instr, cpu.PC, _is_delay_slot);                        \
  }                                                                            \
  local_count++;                                                               \
  DISPATCH();

    uint32_t instr;
    instr = mem_read_32(cpu.PC);
    goto *top_labels[(instr >> 26) & 0x3F];

    OPCODE_LIST(DEFINE_LABEL)

    /* SPECIAL (opcode 0x00): second-level computed-goto on funct.
     * Pure forwarder - the delay-slot bookkeeping is done by the r_* get
     * labels below through the same DEFINE_LABEL body. */
  op_special:
    goto *r_labels[instr & 0x3F];

    RTYPE_LIST(DEFINE_LABEL)

    /* Shared fallback for the unlisted: unimplemented SPECIAL functs
     * (second level) and any top-level opcode outside OPCODE_LIST.
     * Treat the instruction as a NOP and move on. */
  op_skip:
    cpu.PC += 4;
    local_count++;
    DISPATCH();

  emu_done:
    /* Final sync so cpu_fast_get_count() returns the correct total. */
    atomic_store_explicit(&g_insn_count, local_count, memory_order_relaxed);
  }
#else
  /* Portable switch-dispatch path (non GCC/Clang). */
  uint64_t local_count = 0;
  while (true) {
    /* Apply pending-branch target only once the delay-slot instruction has
     * finished. */
    if (cpu.pending_branch && !cpu.branch_delay_slot) {
      cpu.PC = cpu.pending_branch_target;
      cpu.pending_branch = false;
    }

    uint32_t instr = mem_read_32(cpu.PC);
    bool _is_delay_slot = cpu.branch_delay_slot;
    cpu.branch_delay_slot = false;

    cpu.PC = cpu_fast_portable_dispatch(&cpu, instr, cpu.PC, _is_delay_slot);
    local_count++;

    if ((local_count & CHECK_INTERVAL) == 0) {
      atomic_store_explicit(&g_insn_count, local_count, memory_order_relaxed);
      if (!host_is_running())
        break;
    }
  }
  atomic_store_explicit(&g_insn_count, local_count, memory_order_relaxed);
#endif
}

const cpu_interpreter_t g_interp_fast = {
    .name = "FAST",
    .get_status = cpu_fast_get_status,
    .execute = cpu_fast_execute,
    .get_count = cpu_fast_get_count,
    .reset_count = cpu_fast_reset_count,
    .get_exit_code = cpu_fast_get_exit_code,
};
