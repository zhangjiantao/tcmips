#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Unified interpreter interface.
 *
 * All CPU implementations (fast, debug, …) expose the same vtable so that
 * host.c can pick an interpreter with a single pointer.  The functions are
 * intentionally small: initialise CPU state, run, read counters / exit code.
 *
 * The mirror has already been zeroed and the guest image loaded through
 * memory_init() before execute() is called, so the interpreter only has
 * to reset its own CPU state and enter the dispatch loop.
 */
typedef struct cpu_interpreter {
  const char *name;

  const char *(*get_status)(void);

  /** Reset CPU state and run until the guest issues BREAK(0x3ff) or
   *  host_stop_running() is observed. */
  void (*execute)(void);

  /** Thread-safe instruction counter. */
  uint64_t (*get_count)(void);
  void (*reset_count)(void);

  /** Exit code captured from the last guest BREAK(0x3ff) instruction.
   *  Only valid after execute() has returned. */
  uint32_t (*get_exit_code)(void);
} cpu_interpreter_t;

typedef struct {
  uint32_t GPR[32];
  uint32_t PC;
  uint32_t HI;
  uint32_t LO;

  /* Branch-pending state.
   * pending_branch: a taken branch is waiting for its delay slot to finish.
   * pending_branch_target: the target to jump to after the delay slot.
   * branch_delay_slot: the next instruction to dispatch is a branch/jump
   *   delay slot; do not apply pending_branch until after it executes. */
  bool pending_branch;
  uint32_t pending_branch_target;
  bool branch_delay_slot;
} MIPS_CPU;

typedef enum {
  REG_ZERO = 0, // $0
  REG_AT = 1,   // $at
  REG_V0 = 2,   // $v0
  REG_V1 = 3,   // $v1
  REG_A0 = 4,   // $a0
  REG_A1 = 5,   // $a1
  REG_A2 = 6,   // $a2
  REG_A3 = 7,   // $a3
  REG_T0 = 8,   // $t0
  REG_T1 = 9,   // $t1
  REG_T2 = 10,  // $t2
  REG_T3 = 11,  // $t3
  REG_T4 = 12,  // $t4
  REG_T5 = 13,  // $t5
  REG_T6 = 14,  // $t6
  REG_T7 = 15,  // $t7
  REG_S0 = 16,  // $s0
  REG_S1 = 17,  // $s1
  REG_S2 = 18,  // $s2
  REG_S3 = 19,  // $s3
  REG_S4 = 20,  // $s4
  REG_S5 = 21,  // $s5
  REG_S6 = 22,  // $s6
  REG_S7 = 23,  // $s7
  REG_T8 = 24,  // $t8
  REG_T9 = 25,  // $t9
  REG_K0 = 26,  // $k0
  REG_K1 = 27,  // $k1
  REG_GP = 28,  // $gp
  REG_SP = 29,  // $sp
  REG_FP = 30,  // $fp / $s8
  REG_RA = 31   // $ra
} MipsReg;

/* ==================================================================== */
/*  Opcode enumeration                                                   */
/* ==================================================================== */

typedef enum {
  OP_SPECIAL = 0x00,
  OP_REGIMM = 0x01,
  OP_J = 0x02,
  OP_JAL = 0x03,
  OP_BEQ = 0x04,
  OP_BNE = 0x05,
  OP_BLEZ = 0x06,
  OP_BGTZ = 0x07,
  OP_ADDIU = 0x09,
  OP_SLTI = 0x0A,
  OP_SLTIU = 0x0B,
  OP_ANDI = 0x0C,
  OP_ORI = 0x0D,
  OP_XORI = 0x0E,
  OP_LUI = 0x0F,
  OP_SPECIAL2 = 0x1C,
  OP_SPECIAL3 = 0x1F,
  OP_LB = 0x20,
  OP_LH = 0x21,
  OP_LW = 0x23,
  OP_LBU = 0x24,
  OP_LHU = 0x25,
  OP_SB = 0x28,
  OP_SH = 0x29,
  OP_SW = 0x2B
} Opcode;

/* R-Type functions. */
typedef enum {
  FUN_SLL = 0x00,
  FUN_SRL = 0x02,
  FUN_SRA = 0x03,
  FUN_SLLV = 0x04,
  FUN_SRLV = 0x06,
  FUN_SRAV = 0x07,
  FUN_JR = 0x08,
  FUN_JALR = 0x09,
  FUN_MOVZ = 0x0A,
  FUN_MOVN = 0x0B,
  FUN_SYSCALL = 0x0C,
  FUN_BREAK = 0x0D,
  FUN_MFHI = 0x10,
  FUN_MTHI = 0x11,
  FUN_MFLO = 0x12,
  FUN_MTLO = 0x13,
  FUN_MULT = 0x18,
  FUN_MULTU = 0x19,
  FUN_DIV = 0x1A,
  FUN_DIVU = 0x1B,
  FUN_ADDU = 0x21,
  FUN_SUBU = 0x23,
  FUN_AND = 0x24,
  FUN_OR = 0x25,
  FUN_XOR = 0x26,
  FUN_NOR = 0x27,
  FUN_SLT = 0x2A,
  FUN_SLTU = 0x2B
} SpecialFunct;

typedef enum { FUN2_MUL = 0x02 } Special2Funct;

typedef enum { FUN3_BEXT = 0x20 } Special3Funct;

typedef enum { SA_WSBH = 0x02, SA_SEB = 0x10, SA_SEH = 0x18 } Special3Sa;

typedef enum { RT_BLTZ = 0x00, RT_BGEZ = 0x01 } RegImmRt;

/* ── Instruction field helpers (shared by both interpreters) ───────── */

static inline uint32_t extract_rs(uint32_t instr) {
  return (instr >> 21) & 0x1F;
}

static inline uint32_t extract_rt(uint32_t instr) {
  return (instr >> 16) & 0x1F;
}

static inline uint32_t extract_rd(uint32_t instr) {
  return (instr >> 11) & 0x1F;
}

static inline uint32_t extract_sa(uint32_t instr) {
  return (instr >> 6) & 0x1F;
}

static inline uint32_t extract_break_code(uint32_t instr) {
  return (instr >> 16) & 0x3FF;
}

static inline uint32_t extract_funct(uint32_t instr) { return instr & 0x3F; }

static inline uint16_t extract_imm(uint32_t instr) { return (uint16_t)instr; }

static inline uint32_t extract_target(uint32_t instr) {
  return instr & 0x3FFFFFF;
}

static inline int32_t sign_extend_imm(uint32_t instr) {
  return (int32_t)(int16_t)(uint16_t)instr;
}

static inline uint32_t sign_extend_16(uint16_t val) {
  return (uint32_t)(int16_t)val;
}

static inline uint32_t sign_extend_8(uint8_t val) {
  return (uint32_t)(int8_t)val;
}

#endif // CPU_H
