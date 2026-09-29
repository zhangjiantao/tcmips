/*
 * JIT CPU Interpreter — SLJIT based
 *
 * Translates guest MIPS instructions into native x86-64 / AArch64 code at
 * run time using SLJIT (a CPU-independent, stack-less just-in-time compiler).
 * One C source covers both host architectures with no hand-written machine
 * code.
 *
 * Design
 * ------
 *   • Guest general-purpose registers stay in memory.  Each compiled block
 *     addresses the 32 GPRs (and HI/LO) through a base register (BASE_REG)
 *     holding &g_jit_cpu.  The MIPS_CPU layout is the shared definition from
 *     cpu.h, so the JIT state is byte-compatible with the debug interpreter
 *     and the RSP stub.
 *
 *   • Guest RAM is addressed through MEM_REG = &g_emu_memory[0].  Load/store
 *     compute an effective byte address as (MEM_REG + guest offset) and use
 *     SLJIT_MOV{32,}{_U8,_S8,_U16,_S16} for width/sign handling.
 *
 *   • Delay slots are folded into the block.  A branch/jump causes the
 *     following delay-slot instruction to be translated into the same block,
 *     after which the block computes its next guest PC and exits.  This keeps
 *     MIPS delay-slot semantics inside a single block.
 *
 *   • SYSCALL and the exit BREAK(0x3ff) leave the compiled code into small C
 *     helpers (peripheral.c syscall(), and jit_break for the guest exit).
 *
 *   • Translated code is cached per guest PC in a direct table (pc/4 index,
 *     shared with the function-entry marks of the function-level translator)
 *     and is touched only from the (single) CPU thread, so no locking is
 *     needed.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "host.h"
#include "memory.h"
#include "peripheral.h"
#include "platform.h"

#include "sljit/sljitLir.h"

/* ==================================================================== */
/*  Shared JIT state                                                    */
/* ==================================================================== */

static MIPS_CPU g_jit_cpu;     /* guest register file                    */
static uint32_t g_jit_next_pc; /* written by a block on exit             */

uint64_t g_jit_insn_count = 0;
static uint32_t g_jit_exit_code;
static volatile bool g_cpu_running = false;

static void jit_set_exit_code(uint32_t v) { g_jit_exit_code = v; }

static void jit_reset_exit_code(void) { g_jit_exit_code = 0; }

static uint32_t jit_get_exit_code(void) { return g_jit_exit_code; }

static uint64_t jit_get_count(void) { return g_jit_insn_count; }

static void jit_reset_count(void) { g_jit_insn_count = 0; }

static const char *jit_get_status(void) {
  return (host_is_running() && g_cpu_running) ? "RUNNING" : "STOPPED";
}

/* ==================================================================== */
/*  Block cache                                                         */
/* ==================================================================== */

/* A linkable block exit.  A block edge whose successor guest PC is known at
 * translate time (j/jal, and both directions of a conditional branch) is
 * compiled into a "return_to" tail sequence: restore the block's callee-saved
 * registers / stack frame, then jump through a per-exit patch slot.  The slot
 * is a writable function pointer that we update to (a) the successor block's
 * entry once it is translated, or (b) jit_link_resolve when it is not yet
 * available.  Control therefore flows block-to-block without re-entering the
 * dispatcher, except for dynamic targets (jr/jalr) and syscall/break. */
#define JIT_MAX_EXITS 4

typedef struct {
  struct sljit_jump *jump; /* jump IR node (valid until compiler freed)   */
  uint32_t target_pc;      /* target guest PC                              */
  sljit_uw jump_addr;      /* encoding address (valid after generate_code) */
} jit_exit_t;

typedef struct {
  sljit_sw executable_offset; /* needed for sljit_set_jump_addr later       */
  uint32_t exit_count;
  jit_exit_t exits[JIT_MAX_EXITS];
} jit_block_info_t;

typedef struct {
  void *code;
#ifdef JIT_ENABLE_FUNCTION_LEVEL_TRANSLATION
  bool fentry; /* marks a suspected function start (FLT build only) */
#endif
} jit_block_t;

static jit_block_t *g_jit_direct_table;
static jit_block_info_t *g_jit_direct_table_info;
static uint32_t g_jit_direct_table_entries; /* number of slots (pc_end/4)    */

/* Backpatch registry: maps a "not yet translated" target guest PC to the
 * list of (block, exit-index) jump points waiting to be retargeted at it. */
typedef struct jit_backpatch {
  uint32_t target_pc;
  jit_block_t *blk;
  jit_block_info_t *blk_info;
  uint32_t exit_idx;
  struct jit_backpatch *next;
} jit_backpatch_t;

static jit_backpatch_t **g_jit_backpatch; /* indexed by target_pc / 4        */

static void jit_direct_table_clear(void) {
  if (g_jit_backpatch) {
    for (uint32_t i = 0; i < g_jit_direct_table_entries; i++) {
      jit_backpatch_t *e = g_jit_backpatch[i];
      while (e) {
        jit_backpatch_t *n = e->next;
        free(e);
        e = n;
      }
    }
    free(g_jit_backpatch);
    g_jit_backpatch = NULL;
  }

  if (g_jit_direct_table) {
    for (uint32_t i = 0; i < g_jit_direct_table_entries; i++) {
      if (g_jit_direct_table[i].code)
        sljit_free_code(g_jit_direct_table[i].code, NULL);
    }

    free(g_jit_direct_table);
    g_jit_direct_table = NULL;
  }

  if (g_jit_direct_table_info) {
    free(g_jit_direct_table_info);
    g_jit_direct_table_info = NULL;
  }

  g_jit_direct_table_entries = 0;
}

static void jit_direct_table_init(uint32_t pc_end) {
  jit_direct_table_clear();
  uint32_t entries = pc_end / 4;
  g_jit_direct_table = (jit_block_t *)calloc(entries, sizeof(jit_block_t));
  g_jit_direct_table_info =
      (jit_block_info_t *)calloc(entries, sizeof(jit_block_info_t));
  g_jit_backpatch =
      (jit_backpatch_t **)calloc(entries, sizeof(jit_backpatch_t *));
  g_jit_direct_table_entries = entries;
}

static jit_block_t *jit_direct_table_lookup(uint32_t pc) {
  return &g_jit_direct_table[pc / 4];
}

static jit_block_info_t *jit_direct_table_lookup_info(uint32_t pc) {
  return &g_jit_direct_table_info[pc / 4];
}

/* ==================================================================== */
/*  Translation context                                                  */
/* ==================================================================== */

#define BASE_REG SLJIT_S0 /* &g_jit_cpu                     */
#define NEXT_REG SLJIT_S1 /* &g_jit_next_pc                 */
#define MEM_REG SLJIT_S2  /* &g_emu_memory[0]               */

#define OFF_GPR offsetof(MIPS_CPU, GPR)
#define OFF_HI offsetof(MIPS_CPU, HI)
#define OFF_LO offsetof(MIPS_CPU, LO)

typedef struct {
  struct sljit_compiler *compiler;
  uint32_t insn_count;
  jit_block_t
      *blk; /* block being filled; NULL during function-level translation */
  jit_block_info_t
      *blk_info; /* likewise NULL (FLT does not use link metadata) */
} jit_ctx_t;

#ifdef JIT_ENABLE_REG_BINDING
/* Guest GPR -> host saved register.  0 means "not bound" (SLJIT saved
 * registers are all >= SLJIT_S0, which is > 0). */
static sljit_s32 g_jit_reg_map[32] BSS_SECTION;

/* Number of guest GPRs pinned into host *callee-saved* registers.  Saved
 * registers survive a C call (icall for syscall/break), so no spill/reload is
 * needed around syscall — only entry (load) and every exit (store). */
#define JIT_REG_BINDING_COUNT 5

static void jit_reg_map_init(void) {
  g_jit_reg_map[REG_AT] = SLJIT_S3;
  g_jit_reg_map[REG_V0] = SLJIT_S4;
  g_jit_reg_map[REG_V1] = SLJIT_S5;
  g_jit_reg_map[REG_A0] = SLJIT_S6;
  g_jit_reg_map[REG_A1] = SLJIT_S7;
}

static sljit_s32 jit_reg_binding_get(uint32_t r) { return g_jit_reg_map[r]; }

static void jit_reg_binding_load_all(struct sljit_compiler *c) {
  for (uint32_t r = 0; r < 32; r++) {
    sljit_s32 slr = g_jit_reg_map[r];
    if (slr) {
      sljit_emit_op1(c, SLJIT_MOV_U32, slr, 0, SLJIT_MEM1(BASE_REG),
                     (sljit_sw)(OFF_GPR + r * 4));
    }
  }
}

static void jit_reg_binding_store_all(struct sljit_compiler *c) {
  for (uint32_t r = 0; r < 32; r++) {
    sljit_s32 slr = g_jit_reg_map[r];
    if (slr) {
      sljit_emit_op1(c, SLJIT_MOV_U32, SLJIT_MEM1(BASE_REG),
                     (sljit_sw)(OFF_GPR + r * 4), slr, 0);
    }
  }
}

#else
#define JIT_REG_BINDING_COUNT 0

static sljit_s32 jit_reg_binding_get(uint32_t r) {
  (void)r;
  return 0;
}

static void jit_reg_binding_load_all(struct sljit_compiler *c) { (void)c; }

static void jit_reg_binding_store_all(struct sljit_compiler *c) { (void)c; }

static void jit_reg_map_init(void) {}

#endif

/* ── Guest-instruction counting ─────────────────────────────────────────
 *
 * g_jit_insn_count is updated at *control-flow edges*, not at retirement.
 * Every compiled unit (block, or BB inside a function-level translation)
 * terminates in one or more edges (straight exit, branch arms, epilogue,
 * intra-function label jump) and every dynamic execution path crosses
 * exactly one edge per BB visited.  At each edge the emitted native code
 * adds the *static* number of guest instructions along that edge, so the
 * running total is exact regardless of guest loops: loops re-enter their
 * BB through an edge and pay for it through that edge's counter add.
 *
 * This works only if the static count obeys one invariant:
 *
 *   every guest instruction is counted exactly once per static copy of
 *   the translation, i.e. an instruction translated twice (the delay slot
 *   of a branch appears in BOTH branch arms) contributes 1, not 2.
 *
 * Callers keep that invariant by subtracting insn_count once after the
 * second delay-slot translation (see decode_branch and the FLT branch
 * arm). */

/* Emit "g_jit_insn_count += sn" for one control-flow edge.  @p sn is the
 * static guest-instruction count along this edge, computed by the caller
 * at emission time (a compile-time constant folded into the code). */
static void jit_emit_edge_count(jit_ctx_t *ctx, sljit_sw sn) {
  struct sljit_compiler *c = ctx->compiler;

  if (sn <= 0)
    return;

  sljit_emit_op1(c, SLJIT_MOV_P, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)&g_jit_insn_count);
  sljit_emit_op2(c, SLJIT_ADD, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_MEM1(SLJIT_R0), 0,
                 SLJIT_IMM, sn);
}

/* Whole-path shortcut for single-path block exits: at those edges the
 * entire block's translated instruction sequence has executed, so the
 * edge's static count is simply ctx->insn_count. */
static void jit_emit_insn_count(jit_ctx_t *ctx) {
  jit_emit_edge_count(ctx, (sljit_sw)ctx->insn_count);
}

#ifdef JIT_ENABLE_FUNCTION_LEVEL_TRANSLATION
/* ── Function-level translation ───────────────────────────────────────
 *
 * In addition to block-level translation (jit_translate_block), whole functions
 * can be translated with a single sljit compiler so that:
 *   • the callee-saved registers (BASE/NEXT/MEM) and stack frame are set up
 *     once per function, not once per basic block;
 *   • control flow between the function's basic blocks is a compile-time
 *     `jmp label` (no runtime backpatching).
 *
 * A function is discovered by scanning the .text image for `jal` call
 * targets plus the initial entry PC.  Each entry is translated lazily when
 * the dispatcher first reaches it (jit_translate_function).  Any PC that is
 * not a function entry — e.g. an indirect jr/jalr landing point — still falls
 * back to block-level translation, so block-level code remains the safety net
 * for everything the function scanner cannot statically cover. */

/* .text scan range [g_func_text_base, g_func_text_end). */
static uint32_t g_func_text_base;
static uint32_t g_func_text_end;

static bool jit_is_func_entry(uint32_t pc) {
  return g_jit_direct_table[pc / 4].fentry;
}

/* Return the smallest function entry > pc, or g_func_text_end if none. */
static uint32_t jit_next_func_entry(uint32_t pc) {
  for (uint32_t i = (pc / 4) + 1; i < (g_func_text_end / 4); i++)
    if (g_jit_direct_table[i].fentry)
      return i * 4;
  return g_func_text_end;
}

/* Insert `pc` into the entry set (build phase only). */
static void jit_func_entries_add(uint32_t pc) {
  g_jit_direct_table[pc / 4].fentry = true;
}

/* Scan [text_base, text_end) and collect every `jal` target as a function
 * entry, plus the explicit initial entry PC.  Called once per run before any
 * translation.  The image is assumed read-only (no SMC), so this static scan
 * stays valid for the whole run. */
static void jit_func_entries_init(uint32_t text_base, uint32_t text_end,
                                  uint32_t initial_pc) {
  g_func_text_base = text_base;
  g_func_text_end = text_end;

  /* The initial entry (boot stub -> __tcm_start) is always a function start,
   * whether or not anything jals to it. */
  jit_func_entries_add(initial_pc);
  jit_func_entries_add(text_base);

  for (uint32_t pc = text_base; pc + 4 <= text_end; pc += 4) {
    uint32_t instr = mem_read_32(pc);
    uint32_t opcode = (instr >> 26) & 0x3F;
    if (opcode == OP_JAL) {
      uint32_t target = ((pc + 4) & 0xF0000000) | (extract_target(instr) << 2);
      if (target >= text_base && target < text_end)
        jit_func_entries_add(target);
    }
  }
}

#else

static void *jit_translate_function(uint32_t entry) {
  (void)entry;
  return NULL;
}

static void jit_func_entries_init(uint32_t text_base, uint32_t text_end,
                                  uint32_t initial_pc) {
  (void)text_base;
  (void)text_end;
  (void)initial_pc;
}

#endif

/* Load guest GPR i into scratch register dst (low 32 bits). */
static inline void jit_load_gpr(struct sljit_compiler *c, sljit_s32 dst,
                                uint32_t i) {
  if (i == 0) {
    sljit_emit_op1(c, SLJIT_MOV_P, dst, 0, SLJIT_IMM, 0);
    return;
  }

  sljit_s32 slr = jit_reg_binding_get(i);
  if (slr) { // NOLINT
    sljit_emit_op1(c, SLJIT_MOV_P, dst, 0, slr, 0);
    return;
  }

  sljit_emit_op1(c, SLJIT_MOV_U32, dst, 0, SLJIT_MEM1(BASE_REG),
                 (sljit_sw)(OFF_GPR + i * 4));
}

/* Store low 32 bits of scratch register src into guest GPR i. */
static inline void jit_store_gpr(struct sljit_compiler *c, uint32_t i,
                                 sljit_s32 src) {
  if (i == 0)
    return;

  sljit_s32 slr = jit_reg_binding_get(i);
  if (slr) { // NOLINT
    sljit_emit_op1(c, SLJIT_MOV_P, slr, 0, src, 0);
    return;
  }

  sljit_emit_op1(c, SLJIT_MOV_U32, SLJIT_MEM1(BASE_REG),
                 (sljit_sw)(OFF_GPR + i * 4), src, 0);
}

/* Emit block exit: store next guest PC into g_jit_next_pc, then return to
 * the dispatcher.  BREAK(0x3ff) terminates the guest through
 * host_stop_running() inside the jit_break helper, so no extra done-flag is
 * written on block exit. */
static void jit_emit_exit(jit_ctx_t *ctx, uint32_t next_pc) {
  struct sljit_compiler *c = ctx->compiler;

  jit_reg_binding_store_all(c);
  jit_emit_insn_count(ctx);

  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)next_pc);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);
  sljit_emit_return_void(c);
}

/* Resolve stub: entered via a link-exit jump when the target block is not yet
 * translated.  The link-exit sequence has already stored the successor guest
 * PC into g_jit_next_pc and released this block's stack frame (keeping the
 * dispatcher return address on the stack), so returning here simply resumes
 * cpu_jit_execute — which translates the missing block and, via
 * jit_backpatch_resolve, rewrites the jump to enter it directly next time. */
static void jit_link_resolve(void) { /* nothing: g_jit_next_pc already set */ }

/* Emit a linkable block exit to `target_pc`:
 *
 *   [g_jit_next_pc] <- target_pc              (compile-time constant)
 *   restore frame; jmp <rewritable>           (sljit_emit_call + CALL_RETURN)
 *
 * The call-with-CALL_RETURN is a tail-jump: it releases the callee-saved
 * registers / stack frame (is_return_to == 0, so the dispatcher return
 * address stays on the stack) and emits a rewritable direct jump.  The jump
 * target is patched later (sljit_set_jump_addr) to the successor block entry,
 * or left at jit_link_resolve until that block is translated. */
static void jit_emit_link_exit(jit_ctx_t *ctx, uint32_t target_pc) {
  struct sljit_compiler *c = ctx->compiler;

  jit_block_info_t *info = ctx->blk_info;
  if (info->exit_count >= JIT_MAX_EXITS) {
    /* Too many linkable exits: fall back to a dispatcher return (safe).
     * jit_emit_exit() accounts the instruction count for this edge. */
    jit_emit_exit(ctx, target_pc);
    return;
  }

  jit_emit_insn_count(ctx);

  jit_exit_t *e = &info->exits[info->exit_count++];
  e->target_pc = target_pc;

  /* Write back bound guest registers before releasing the stack frame (which
   * pops the host saved registers that hold them). */
  jit_reg_binding_store_all(c);

  /* Store the next guest PC so the resolve stub / dispatcher can resume. */
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)target_pc);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);

  /* Tail jump: release stack frame (keep return address) + rewritable jump.
   * arg_types == 0 means a void return, matching our enter's last_return. */
  e->jump = sljit_emit_call(
      c, SLJIT_CALL | SLJIT_CALL_RETURN | SLJIT_REWRITABLE_JUMP, 0);
  sljit_set_target(e->jump, (sljit_uw)(uintptr_t)&jit_link_resolve);
}

/* ==================================================================== */
/*  Exit linking: register + resolve linkable jump encodings             */
/* ==================================================================== */

/* Register `blk->exits[i]` in the backpatch table keyed by target_pc, so the
 * jump can be retargeted once that guest PC is translated. */
static bool jit_backpatch_add(jit_block_t *blk, jit_block_info_t *blk_info,
                              uint32_t i) {
  uint32_t tgt = blk_info->exits[i].target_pc;
  if (tgt / 4 >= g_jit_direct_table_entries)
    return false;

  jit_backpatch_t *e = (jit_backpatch_t *)malloc(sizeof(jit_backpatch_t));
  if (!e)
    return false;
  e->target_pc = tgt;
  e->blk = blk;
  e->blk_info = blk_info;
  e->exit_idx = i;
  e->next = g_jit_backpatch[tgt / 4];
  g_jit_backpatch[tgt / 4] = e;
  return true;
}

/* Resolve a single linkable exit: retarget the jump at the successor entry if
 * already translated, otherwise leave it at the resolve stub and register a
 * backpatch for a later fixup. */
static void jit_link_resolve_exit(jit_block_t *blk,
                                  jit_block_info_t *block_info, uint32_t i) {
  jit_exit_t *e = &block_info->exits[i];
  uint32_t tgt = e->target_pc;

  jit_block_t *tb = NULL;
  if (tgt / 4 < g_jit_direct_table_entries)
    tb = &g_jit_direct_table[tgt / 4];

  if (tb && tb->code) {
    sljit_set_jump_addr(e->jump_addr, (sljit_uw)(uintptr_t)tb->code,
                        block_info->executable_offset);
  } else {
    sljit_set_jump_addr(e->jump_addr, (sljit_uw)(uintptr_t)&jit_link_resolve,
                        block_info->executable_offset);
    jit_backpatch_add(blk, block_info, i);
  }
}

/* Called after `target` (keyed by `start_pc`) has been translated: retarget
 * every pending jump that waits on `start_pc` to enter it directly. */
static void jit_backpatch_resolve(uint32_t start_pc) {
  if (start_pc / 4 >= g_jit_direct_table_entries)
    return;

  jit_block_t *target = jit_direct_table_lookup(start_pc);
  if (!target->code)
    return;

  jit_backpatch_t *e = g_jit_backpatch[start_pc / 4];
  g_jit_backpatch[start_pc / 4] = NULL;

  for (; e; e = e->next) {
    jit_exit_t *ex = &e->blk_info->exits[e->exit_idx];
    sljit_set_jump_addr(ex->jump_addr, (sljit_uw)(uintptr_t)target->code,
                        e->blk_info->executable_offset);
  }
  /* Note: the linked list nodes are intentionally leaked-until-free; the whole
   * backpatch table is torn down in jit_direct_table_clear(). */
}

/* ==================================================================== */
/*  C helpers invoked from compiled code                                 */
/* ==================================================================== */

static uint32_t jit_syscall(uint32_t nr, uint32_t rd, uint32_t a0,
                            uint32_t a1) {
  return syscall(nr, rd, a0, a1);
}

static uint32_t jit_break(uint32_t code, uint32_t reason) {
  if (code == 0x3ff && host_is_running()) {
    jit_set_exit_code(g_jit_cpu.GPR[5]);
    if (reason == 0)
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR EXIT, code=%u !!\n",
              jit_get_exit_code());
    else
      fprintf(stderr, "\n\n!! TCMIPS EMULATOR ABORT, reason=%u, code=%u !!\n",
              reason, jit_get_exit_code());
    host_stop_running();
    g_cpu_running = false;
    return 1;
  }
  return 0;
}

/* ==================================================================== */
/*  Instruction translators (straight-line; no control flow)            */
/* ==================================================================== */

static void tr_addiu(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  sljit_emit_op2(c, SLJIT_ADD32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)sign_extend_imm(instr));
  jit_store_gpr(c, extract_rt(instr), SLJIT_R0);
}

static void tr_slti(jit_ctx_t *ctx, uint32_t instr, bool uns) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  struct sljit_jump *j;
  if (uns)
    j = sljit_emit_cmp(c, SLJIT_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM,
                       (sljit_sw)(sljit_s32)sign_extend_imm(instr));
  else
    j = sljit_emit_cmp(c, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM,
                       (sljit_sw)(sljit_s32)sign_extend_imm(instr));

  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
  struct sljit_jump *skip = sljit_emit_jump(c, SLJIT_JUMP);
  struct sljit_label *l_true = sljit_emit_label(c);
  sljit_set_label(j, l_true);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 1);
  struct sljit_label *l_end = sljit_emit_label(c);
  sljit_set_label(skip, l_end);
  jit_store_gpr(c, extract_rt(instr), SLJIT_R0);
}

static void tr_andixor(jit_ctx_t *ctx, uint32_t instr, sljit_s32 op) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  sljit_emit_op2(c, op, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)extract_imm(instr));
  jit_store_gpr(c, extract_rt(instr), SLJIT_R0);
}

static void tr_lui(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)(extract_imm(instr) << 16));
  jit_store_gpr(c, extract_rt(instr), SLJIT_R0);
}

static void tr_load(jit_ctx_t *ctx, uint32_t instr, sljit_s32 mov_op) {
  struct sljit_compiler *c = ctx->compiler;

  /* rt = mem[GPR[rs] + sign_extend(imm)]  (byte-addressable guest RAM) */
  jit_load_gpr(c, SLJIT_R0, extract_rs(instr)); /* R0 = GPR[rs] */
  sljit_emit_op2(c, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)sign_extend_imm(instr)); /* R0 += offset */
  sljit_emit_op2(c, SLJIT_ADD, SLJIT_R0, 0, MEM_REG, 0, SLJIT_R0,
                 0); /* R0 += RAM base */
  sljit_emit_op1(c, mov_op, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), 0);
  jit_store_gpr(c, extract_rt(instr), SLJIT_R1);
}

static void tr_store(jit_ctx_t *ctx, uint32_t instr, sljit_s32 st_op) {
  struct sljit_compiler *c = ctx->compiler;

  /* mem[GPR[rs] + sign_extend(imm)] = GPR[rt]  (low bits per st_op) */
  jit_load_gpr(c, SLJIT_R0, extract_rs(instr)); /* R0 = GPR[rs] */
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr)); /* R1 = GPR[rt] (value) */
  sljit_emit_op2(c, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)sign_extend_imm(instr)); /* R0 += offset */
  sljit_emit_op2(c, SLJIT_ADD, SLJIT_R0, 0, MEM_REG, 0, SLJIT_R0,
                 0); /* R0 += RAM base */
  sljit_emit_op1(c, st_op, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R1, 0);
}

static void tr_alu3(jit_ctx_t *ctx, uint32_t instr, sljit_s32 op) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
  sljit_emit_op2(c, op, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

static void tr_slt(jit_ctx_t *ctx, uint32_t instr, bool uns) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
  struct sljit_jump *j;
  if (uns)
    j = sljit_emit_cmp(c, SLJIT_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0);
  else
    j = sljit_emit_cmp(c, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 0);
  struct sljit_jump *skip = sljit_emit_jump(c, SLJIT_JUMP);
  struct sljit_label *l = sljit_emit_label(c);
  sljit_set_label(j, l);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, 1);
  struct sljit_label *e = sljit_emit_label(c);
  sljit_set_label(skip, e);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

static void tr_nor(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
  sljit_emit_op2(c, SLJIT_OR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
  sljit_emit_op2(c, SLJIT_XOR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                 (sljit_sw)0xFFFFFFFF);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

static void tr_shift(jit_ctx_t *ctx, uint32_t instr, sljit_s32 op,
                     bool use_sa) {
  struct sljit_compiler *c = ctx->compiler;

  uint32_t rd = extract_rd(instr);
  if (rd == 0)
    return;

  jit_load_gpr(c, SLJIT_R0, extract_rt(instr));
  if (use_sa) {
    sljit_emit_op2(c, op, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                   (sljit_sw)extract_sa(instr));
  } else {
    jit_load_gpr(c, SLJIT_R1, extract_rs(instr));
    sljit_emit_op2(c, op, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
  }
  jit_store_gpr(c, rd, SLJIT_R0);
}

static void tr_mfhi(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  /* rd = HI */
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM1(BASE_REG),
                 (sljit_sw)OFF_HI);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

static void tr_mflo(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  /* rd = LO */
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_MEM1(BASE_REG),
                 (sljit_sw)OFF_LO);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

static void tr_mthi(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_HI,
                 SLJIT_R0, 0);
}

static void tr_mtlo(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_LO,
                 SLJIT_R0, 0);
}

/* MULT/MULTU: HI:LO = rs * rt (32x32 -> 64-bit, signed/unsigned).
 *
 * sljit's SLJIT_LMUL_UW/SW is a full-word (64x64) multiply whose low word
 * lands in R0 and high word in R1.  Because our operands are 32-bit values,
 * the full product fits in the low 64 bits (i.e. in R0) and R1 is just the
 * sign/zero extension.  We therefore take LO = R0 low 32 bits and
 * HI = R0 >> 32 (the high 32 bits of the product), which never references
 * R1. */
static void tr_mult(jit_ctx_t *ctx, uint32_t instr, bool uns) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
  if (uns) {
    sljit_emit_op0(c, SLJIT_LMUL_UW);
  } else {
    sljit_emit_op1(c, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
    sljit_emit_op1(c, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
    sljit_emit_op0(c, SLJIT_LMUL_SW);
  }
  /* LO = R0 low 32 bits; HI = R0 >> 32 (see header comment). */
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_LO,
                 SLJIT_R0, 0);
  sljit_emit_op2(c, SLJIT_LSHR, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_HI,
                 SLJIT_R1, 0);
}

/* DIV/DIVU: LO = quotient, HI = remainder; leave unchanged if divisor == 0.
 * (SLJIT's DIVMOD is undefined on a zero divisor, hence the skip.) */
static void tr_div(jit_ctx_t *ctx, uint32_t instr, bool uns) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr)); /* dividend */
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr)); /* divisor */

  struct sljit_jump *j_zero =
      sljit_emit_cmp(c, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);

  if (uns) {
    sljit_emit_op0(c, SLJIT_DIVMOD_U32);
  } else {
    sljit_emit_op1(c, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
    sljit_emit_op1(c, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R1, 0);
    sljit_emit_op0(c, SLJIT_DIVMOD_S32);
  }
  /* R0 = quotient (LO), R1 = remainder (HI) */
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_LO,
                 SLJIT_R0, 0);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG), (sljit_sw)OFF_HI,
                 SLJIT_R1, 0);

  struct sljit_label *l_end = sljit_emit_label(c);
  sljit_set_label(j_zero, l_end);
}

/* MOVZ/MOVN: conditional move.
 *   on_zero == true  -> MOVZ rd,rs,rt : if rt == 0 then rd = rs
 *   on_zero == false -> MOVN rd,rs,rt : if rt != 0 then rd = rs */
static void tr_movzn(jit_ctx_t *ctx, uint32_t instr, bool on_zero) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));

  struct sljit_jump *j_skip;
  if (on_zero) /* MOVZ: move when rt == 0, so skip when rt != 0 */
    j_skip = sljit_emit_cmp(c, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
  else /* MOVN: move when rt != 0, so skip when rt == 0 */
    j_skip = sljit_emit_cmp(c, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);

  jit_store_gpr(c, extract_rd(instr), SLJIT_R0); /* rd = rs */

  struct sljit_label *l_end = sljit_emit_label(c);
  sljit_set_label(j_skip, l_end);
}

/* SPECIAL2 MUL (full 32-bit multiply, low result) */
static void tr_mul(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
  jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
  sljit_emit_op2(c, SLJIT_MUL32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
}

/* SPECIAL3 BEXT (WSBH / SEB / SEH).  Return false if the SA is unknown. */
static bool tr_bext(jit_ctx_t *ctx, uint32_t instr) {
  struct sljit_compiler *c = ctx->compiler;

  uint32_t sa = extract_sa(instr);
  jit_load_gpr(c, SLJIT_R0, extract_rt(instr));
  switch (sa) {
  case SA_WSBH:
    sljit_emit_op1(c, SLJIT_REV32, SLJIT_R0, 0, SLJIT_R0, 0);
    sljit_emit_op2(c, SLJIT_ROTR32, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM,
                   (sljit_sw)16);
    break;
  case SA_SEB:
    sljit_emit_op1(c, SLJIT_MOV32_S8, SLJIT_R0, 0, SLJIT_R0, 0);
    break;
  case SA_SEH:
    sljit_emit_op1(c, SLJIT_MOV32_S16, SLJIT_R0, 0, SLJIT_R0, 0);
    break;
  default:
    return false; /* NOP */
  }
  jit_store_gpr(c, extract_rd(instr), SLJIT_R0);
  return true;
}

/* ==================================================================== */
/*  Forward declarations of decode helpers                               */
/* ==================================================================== */

static void decode_special(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                           uint32_t *next_pc, bool *ended);
static void decode_branch(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                          uint32_t *next_pc, bool *ended);
static void decode_jump(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                        uint32_t *next_pc, bool *ended);
static void decode_jr(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                      uint32_t funct, uint32_t *next_pc, bool *ended);
static void decode_syscall(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                           uint32_t *next_pc, bool *ended);
static void decode_break(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                         uint32_t *next_pc, bool *ended);
static void decode_delay_slot(jit_ctx_t *ctx, uint32_t instr);
static bool decode_straight_line(jit_ctx_t *ctx, uint32_t instr);

/* ==================================================================== */
/*  decode_straight_line — lower one non-control-flow instruction        */
/* ==================================================================== */

/* Emit the code for a single guest instruction that has no control-flow
 * effect (immediate ALU, load/store, and the non-branch R-type / SPECIAL2 /
 * SPECIAL3 families).  Returns true if the instruction was recognised and
 * translated, false if it is a control-flow instruction (branch / jump /
 * jr / jalr / syscall / break) or unknown — in which case the caller is
 * responsible for handling it.
 *
 * This is the single source of truth for arithmetic/memory instruction
 * lowering; every decoder routes through here so the per-instruction table
 * exists exactly once. */
static bool decode_straight_line(jit_ctx_t *ctx, uint32_t instr) {
  uint32_t opcode = (instr >> 26) & 0x3F;

  switch (opcode) {
  case OP_ADDIU:
    tr_addiu(ctx, instr);
    return true;
  case OP_SLTI:
    tr_slti(ctx, instr, false);
    return true;
  case OP_SLTIU:
    tr_slti(ctx, instr, true);
    return true;
  case OP_ANDI:
    tr_andixor(ctx, instr, SLJIT_AND32);
    return true;
  case OP_ORI:
    tr_andixor(ctx, instr, SLJIT_OR32);
    return true;
  case OP_XORI:
    tr_andixor(ctx, instr, SLJIT_XOR32);
    return true;
  case OP_LUI:
    tr_lui(ctx, instr);
    return true;
  case OP_LB:
    tr_load(ctx, instr, SLJIT_MOV32_S8);
    return true;
  case OP_LBU:
    tr_load(ctx, instr, SLJIT_MOV32_U8);
    return true;
  case OP_LH:
    tr_load(ctx, instr, SLJIT_MOV32_S16);
    return true;
  case OP_LHU:
    tr_load(ctx, instr, SLJIT_MOV32_U16);
    return true;
  case OP_LW:
    tr_load(ctx, instr, SLJIT_MOV32);
    return true;
  case OP_SB:
    tr_store(ctx, instr, SLJIT_MOV_U8);
    return true;
  case OP_SH:
    tr_store(ctx, instr, SLJIT_MOV_U16);
    return true;
  case OP_SW:
    tr_store(ctx, instr, SLJIT_MOV32);
    return true;
  case OP_SPECIAL2:
    if (extract_funct(instr) == FUN2_MUL)
      tr_mul(ctx, instr);
    return true;
  case OP_SPECIAL3:
    tr_bext(ctx, instr);
    return true;
  case OP_SPECIAL: {
    uint32_t funct = extract_funct(instr);
    switch (funct) {
    case FUN_ADDU:
      tr_alu3(ctx, instr, SLJIT_ADD32);
      return true;
    case FUN_SUBU:
      tr_alu3(ctx, instr, SLJIT_SUB32);
      return true;
    case FUN_AND:
      tr_alu3(ctx, instr, SLJIT_AND32);
      return true;
    case FUN_OR:
      tr_alu3(ctx, instr, SLJIT_OR32);
      return true;
    case FUN_XOR:
      tr_alu3(ctx, instr, SLJIT_XOR32);
      return true;
    case FUN_NOR:
      tr_nor(ctx, instr);
      return true;
    case FUN_SLT:
      tr_slt(ctx, instr, false);
      return true;
    case FUN_SLTU:
      tr_slt(ctx, instr, true);
      return true;
    case FUN_SLL:
      tr_shift(ctx, instr, SLJIT_SHL32, true);
      return true;
    case FUN_SRL: {
      uint32_t rs = extract_rs(instr);
      if (rs == 0)
        tr_shift(ctx, instr, SLJIT_LSHR32, true);
      else if (rs == 1)
        tr_shift(ctx, instr, SLJIT_ROTR32, true);
      return true;
    }
    case FUN_SRA:
      tr_shift(ctx, instr, SLJIT_ASHR32, true);
      return true;
    case FUN_SLLV:
      tr_shift(ctx, instr, SLJIT_MSHL32, false);
      return true;
    case FUN_SRLV: {
      uint32_t sa = extract_sa(instr);
      if (sa == 0)
        tr_shift(ctx, instr, SLJIT_MLSHR32, false);
      else if (sa == 1)
        tr_shift(ctx, instr, SLJIT_ROTR32, false);
      return true;
    }
    case FUN_SRAV:
      tr_shift(ctx, instr, SLJIT_MASHR32, false);
      return true;
    case FUN_MFHI:
      tr_mfhi(ctx, instr);
      return true;
    case FUN_MFLO:
      tr_mflo(ctx, instr);
      return true;
    case FUN_MTHI:
      tr_mthi(ctx, instr);
      return true;
    case FUN_MTLO:
      tr_mtlo(ctx, instr);
      return true;
    case FUN_MULT:
      tr_mult(ctx, instr, false);
      return true;
    case FUN_MULTU:
      tr_mult(ctx, instr, true);
      return true;
    case FUN_DIV:
      tr_div(ctx, instr, false);
      return true;
    case FUN_DIVU:
      tr_div(ctx, instr, true);
      return true;
    case FUN_MOVZ:
      tr_movzn(ctx, instr, true);
      return true;
    case FUN_MOVN:
      tr_movzn(ctx, instr, false);
      return true;
    default:
      /* jr/jalr/syscall/break — control flow, handled by the caller. */
      return false;
    }
  }
  default:
    /* branch (BEQ/BNE/.../REGIMM), J/JAL, or unknown — not handled here. */
    return false;
  }
}

/* ==================================================================== */
/*  decode_and_emit — translate one instruction; manage block flow       */
/* ==================================================================== */

static void decode_and_emit(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                            uint32_t *next_pc, bool *ended) {
  ctx->insn_count++;

  uint32_t opcode = (instr >> 26) & 0x3F;
  *next_pc = pc + 4;
  *ended = false;

  /* Control-flow instructions are dispatched specially; everything else is
   * lowered by the shared decode_straight_line. */
  switch (opcode) {
  case OP_SPECIAL:
    decode_special(ctx, instr, pc, next_pc, ended);
    return;
  case OP_BEQ:
  case OP_BNE:
  case OP_BLEZ:
  case OP_BGTZ:
  case OP_REGIMM:
    decode_branch(ctx, instr, pc, next_pc, ended);
    return;
  case OP_J:
  case OP_JAL:
    decode_jump(ctx, instr, pc, next_pc, ended);
    return;
  default:
    decode_straight_line(ctx, instr);
    return;
  }
}

/* ── SPECIAL (R-type) decode ────────────────────────────────────────── */

/* Only the control-flow R-type instructions are dispatched here; the rest of
 * the R-type family is lowered by the shared decode_straight_line. */
static void decode_special(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                           uint32_t *next_pc, bool *ended) {
  uint32_t funct = extract_funct(instr);
  switch (funct) {
  case FUN_JR:
  case FUN_JALR:
    decode_jr(ctx, instr, pc, funct, next_pc, ended);
    return;
  case FUN_SYSCALL:
    decode_syscall(ctx, instr, pc, next_pc, ended);
    return;
  case FUN_BREAK:
    decode_break(ctx, instr, pc, next_pc, ended);
    return;
  default:
    decode_straight_line(ctx, instr);
    return;
  }
}

/* ── Terminators: JR/JALR, SYSCALL, BREAK ──────────────────────────── */

static void decode_jr(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                      uint32_t funct, uint32_t *next_pc, bool *ended) {
  struct sljit_compiler *c = ctx->compiler;

  uint32_t rs = extract_rs(instr);
  uint32_t rd = extract_rd(instr);

  if (funct == FUN_JALR) {
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG),
                   (sljit_sw)(OFF_GPR + rd * 4), SLJIT_IMM, (sljit_sw)(pc + 8));
  }

  /* Translate the delay-slot instruction at pc + 4. */
  uint32_t ds = mem_read_32(pc + 4);
  decode_delay_slot(ctx, ds);

  /* Exit: next PC = GPR[rs]. */
  jit_emit_insn_count(ctx);
  jit_reg_binding_store_all(c);
  jit_load_gpr(c, SLJIT_R0, rs);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);
  sljit_emit_return_void(c);

  *next_pc = pc + 4;
  *ended = true;
}

static void decode_syscall(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                           uint32_t *next_pc, bool *ended) {
  struct sljit_compiler *c = ctx->compiler;

  uint32_t nr = extract_sa(instr);
  uint32_t rd = extract_rd(instr);
  (void)pc;

  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)nr);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)rd);
  jit_load_gpr(c, SLJIT_R2, 4); /* a0 */
  jit_load_gpr(c, SLJIT_R3, 5); /* a1 */
  sljit_emit_icall(c, SLJIT_CALL, SLJIT_ARGS4(W, W, W, W, W), SLJIT_IMM,
                   (sljit_sw)(uintptr_t)&jit_syscall);
  jit_store_gpr(c, 2, SLJIT_R0);

  *next_pc = pc + 4;
  *ended = false;
}

static void decode_break(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                         uint32_t *next_pc, bool *ended) {
  uint32_t code = extract_break_code(instr);
  struct sljit_compiler *c = ctx->compiler;
  if (code == 0x3ff) {
    jit_emit_insn_count(ctx);
    jit_reg_binding_store_all(c);
    jit_load_gpr(c, SLJIT_R0, 4); /* reason = GPR[4] */
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)code);
    /* jit_break(code, reason): arg1=code(R1), arg2=reason(R0) — reorder to
     * match jit_break(code, reason). */
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R0, 0);
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R1, 0);
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R2, 0);
    sljit_emit_icall(c, SLJIT_CALL, SLJIT_ARGS2(W, W, W), SLJIT_IMM,
                     (sljit_sw)(uintptr_t)&jit_break);

    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)(pc + 8));
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);
    sljit_emit_return_void(c);

    *ended = true;
  } else {
    *next_pc = pc + 4;
    *ended = false;
  }
}

/* ── Conditional branches (BEQ/BNE/BLEZ/BGTZ/REGIMM) with delay slot ─── */

static void decode_branch(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                          uint32_t *next_pc, bool *ended) {
  uint32_t opcode = (instr >> 26) & 0x3F;
  uint32_t target = pc + 4 + ((uint32_t)(int32_t)sign_extend_imm(instr) << 2);
  uint32_t ds = mem_read_32(pc + 4);
  struct sljit_compiler *c = ctx->compiler;

  /* Emit "taken" jump to a label we'll lay out after the not-taken path.  We
   * structure the block as:
   *   if (cond taken) goto TAKEN;
   * NOT_TAKEN: { delay slot; exit(pc+8) }
   * TAKEN:     { delay slot; exit(target) }
   */
  struct sljit_jump *j_taken = NULL;
  switch (opcode) {
  case OP_BEQ:
    jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
    jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
    j_taken =
        sljit_emit_cmp(c, SLJIT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0);
    break;
  case OP_BNE:
    jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
    jit_load_gpr(c, SLJIT_R1, extract_rt(instr));
    j_taken =
        sljit_emit_cmp(c, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0);
    break;
  case OP_BLEZ:
    jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
    j_taken = sljit_emit_cmp(c, SLJIT_SIG_LESS_EQUAL | SLJIT_32, SLJIT_R0, 0,
                             SLJIT_IMM, 0);
    break;
  case OP_BGTZ:
    jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
    j_taken = sljit_emit_cmp(c, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R0, 0,
                             SLJIT_IMM, 0);
    break;
  case OP_REGIMM: {
    uint32_t rt = extract_rt(instr);
    if (rt == RT_BLTZ) {
      jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
      j_taken = sljit_emit_cmp(c, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R0, 0,
                               SLJIT_IMM, 0);
      break;
    } else if (rt == RT_BGEZ) {
      jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
      j_taken = sljit_emit_cmp(c, SLJIT_SIG_GREATER_EQUAL | SLJIT_32, SLJIT_R0,
                               0, SLJIT_IMM, 0);
      break;
    }
    /* unknown REGIMM: NOP, no delay slot */
    *next_pc = pc + 4;
    *ended = false;
    return;
  }
  default:
    return;
  }

  /* Layout: condition-taken jumps to l_taken; condition-not-taken falls
   * through to the not-taken body.  Each body translates the delay slot then
   * emits a linkable exit (a rewritable jump) to its successor, so the two
   * paths do not fall into one another and both branch directions are later
   * chained directly to their target blocks. */
  /* NOT_TAKEN body (fallthrough): */
  decode_delay_slot(ctx, ds);
  jit_emit_link_exit(ctx, pc + 8);

  /* TAKEN body (reached by j_taken): */
  struct sljit_label *l_taken = sljit_emit_label(c);
  sljit_set_label(j_taken, l_taken);
  decode_delay_slot(ctx, ds);
  /* The delay slot is statically translated in BOTH branch arms but every
   * dynamic execution runs only one of them; undo the second count so the
   * per-instruction invariant "counted once per static copy" holds. */
  ctx->insn_count--;
  jit_emit_link_exit(ctx, target);

  *ended = true;
  *next_pc = pc + 4;
}

/* ── J / JAL (unconditional, with delay slot) ──────────────────────── */

static void decode_jump(jit_ctx_t *ctx, uint32_t instr, uint32_t pc,
                        uint32_t *next_pc, bool *ended) {
  uint32_t opcode = (instr >> 26) & 0x3F;
  uint32_t target = ((pc + 4) & 0xF0000000) | (extract_target(instr) << 2);
  uint32_t ds = mem_read_32(pc + 4);
  struct sljit_compiler *c = ctx->compiler;

  if (opcode == OP_JAL) {
    sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG),
                   (sljit_sw)(OFF_GPR + 31 * 4), SLJIT_IMM, (sljit_sw)(pc + 8));
  }

  decode_delay_slot(ctx, ds);
  jit_emit_link_exit(ctx, target);

  *ended = true;
  *next_pc = pc + 4;
}

/* ==================================================================== */
/*  Delay-slot decode                                                     */
/* ==================================================================== */

/* Translate exactly ONE delay-slot guest instruction and advance the
 * instruction counter (the delay slot occupies its dispatch slot even when
 * the instruction turns out to be untranslatable).  Used by every
 * branch/jump lowering, in both block-level and function-level translation.
 * A control-flow instruction in a delay slot is ill-formed MIPS: it is
 * silently ignored here (decode_straight_line returns false without emitting
 * anything), matching the interpreters' tolerant handling. */
static void decode_delay_slot(jit_ctx_t *ctx, uint32_t instr) {
  ctx->insn_count++;
  decode_straight_line(ctx, instr);
}

#ifdef JIT_ENABLE_FUNCTION_LEVEL_TRANSLATION
/* ==================================================================== */
/*  Function translator (whole-function, single compiler)               */
/* ==================================================================== */

/* A basic block within a function being translated: its start PC and the
 * already-emitted entry label (so intra-function jumps can bind to it). */
typedef struct {
  uint32_t pc;
  struct sljit_label *label;
} func_bb_t;

typedef struct {
  jit_ctx_t base;      /* compiler + insn_count (reused by tr_* helpers) */
  uint32_t func_entry; /* entry PC of the function                       */
  uint32_t func_end;   /* first PC outside the function (next entry/end) */
  func_bb_t *bbs;      /* translated basic-block label map               */
  uint32_t bb_count;
  uint32_t bb_cap;
} func_ctx_t;

static func_bb_t *func_find_bb(func_ctx_t *f, uint32_t pc) {
  for (uint32_t i = 0; i < f->bb_count; i++)
    if (f->bbs[i].pc == pc)
      return &f->bbs[i];
  return NULL;
}

static func_bb_t *func_add_bb(func_ctx_t *f, uint32_t pc,
                              struct sljit_label *label) {
  if (f->bb_count == f->bb_cap) {
    f->bb_cap = f->bb_cap ? f->bb_cap * 2 : 16;
    f->bbs = (func_bb_t *)realloc(f->bbs, f->bb_cap * sizeof(func_bb_t));
  }
  f->bbs[f->bb_count].pc = pc;
  f->bbs[f->bb_count].label = label;
  return &f->bbs[f->bb_count++];
}

/* Emit a function-external exit: account this edge's instruction count,
 * store the next guest PC, restore the callee-saved registers / frame and
 * return to the dispatcher.  This is the function's only way to hand control
 * back (jr/jalr, indirect target, cross-function call, break).
 *
 * The epilogue IS a control-flow edge of the current BB.  @p path_count is
 * that edge's static guest-instruction count, computed by the CALLER (the
 * current BB's straight-line prefix + terminator + one delay slot).  It must
 * be computed BEFORE any recursive translation runs, because insn_count is
 * a single emitter-wide accumulator: by the time a second branch arm in the
 * same BB emits, the not-taken arm's recursion has already advanced it, so
 * a deferred (insn_count - bb_start) difference would be polluted.  Callers
 * of func_emit_epilogue / func_emit_jump_to must not count the same edge
 * twice. */
static void func_emit_epilogue(func_ctx_t *f, uint32_t next_pc,
                               sljit_sw path_count) {
  jit_ctx_t *ctx = &f->base;
  struct sljit_compiler *c = ctx->compiler;

  jit_emit_edge_count(ctx, path_count);
  jit_reg_binding_store_all(c);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)next_pc);
  sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);
  sljit_emit_return_void(c);
}

/* Emit an intra-function jump to the block at `target_pc`, translating it on
 * demand.  If `target_pc` lies outside this function it is instead compiled
 * as an external exit (cross-function / out-of-range).  @p path_count is the
 * edge's static guest-instruction count, computed by the caller BEFORE any
 * recursion (see func_emit_epilogue). */
static void func_emit_jump_to(func_ctx_t *f, uint32_t target_pc,
                              sljit_sw path_count);

/* Forward decl for the block translator (mutually recursive). */
static void jit_translate_function_block(func_ctx_t *f, uint32_t start_pc);

/* Translate a single basic block starting at `start_pc`, walking straight-line
 * until a control-flow boundary, then recursively translating successors.
 *
 * Edge counting: the BB's baseline (its straight-line prefix length) is kept
 * in the LOCAL @c bb_start so recursive translations of successor blocks'
 * emission never interact with it.  Every edge leaving this BB must pass an
 * explicitly precomputed path count (never a deferred insn_count difference).
 */
static void jit_translate_function_block(func_ctx_t *f, uint32_t start_pc) {
  /* Guard against out-of-function PCs (cross-function or out of .text).
   * Unreachable today: func_emit_jump_to bounds-checks before recursing and
   * the top-level entry is in-range by construction.  Pass 0 so the edge
   * helper no-ops; a real edge would have to count here as well. */
  if (start_pc < f->func_entry || start_pc >= f->func_end) {
    func_emit_epilogue(f, start_pc, 0);
    return;
  }

  jit_ctx_t *ctx = &f->base;
  struct sljit_compiler *c = ctx->compiler;

  /* Already translated? Bind as a jump and stop recursing. */
  func_bb_t *existing = func_find_bb(f, start_pc);
  if (existing) {
    struct sljit_jump *j = sljit_emit_jump(c, SLJIT_JUMP);
    sljit_set_label(j, existing->label);
    return;
  }

  struct sljit_label *label = sljit_emit_label(c);
  func_add_bb(f, start_pc, label);

  /* Baseline for the edges that leave THIS basic block: their static
   * count is (insn_count - bb_start).  LOCAL on purpose: recursive BB
   * translations keep/save their own frame's baseline, so no field of
   * @p f can be clobbered underneath us. */
  uint32_t bb_start = ctx->insn_count;

  uint32_t pc = start_pc;
  for (;;) {
    uint32_t instr = mem_read_32(pc);
    uint32_t opcode = (instr >> 26) & 0x3F;

    /* ── Conditional branch (BEQ/BNE/BLEZ/BGTZ/REGIMM) ── */
    if (opcode == OP_BEQ || opcode == OP_BNE || opcode == OP_BLEZ ||
        opcode == OP_BGTZ || opcode == OP_REGIMM) {
      uint32_t target =
          pc + 4 + ((uint32_t)(int32_t)sign_extend_imm(instr) << 2);
      uint32_t ds = mem_read_32(pc + 4);
      uint32_t rt = extract_rt(instr);

      /* The branch instruction itself (covers the unknown-REGIMM
       * straight-line case as well). */
      ctx->insn_count++;

      struct sljit_jump *j_taken = NULL;
      bool is_cond = true;
      switch (opcode) {
      case OP_BEQ:
        jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
        jit_load_gpr(c, SLJIT_R1, rt);
        j_taken =
            sljit_emit_cmp(c, SLJIT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0);
        break;
      case OP_BNE:
        jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
        jit_load_gpr(c, SLJIT_R1, rt);
        j_taken = sljit_emit_cmp(c, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0,
                                 SLJIT_R1, 0);
        break;
      case OP_BLEZ:
        jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
        j_taken = sljit_emit_cmp(c, SLJIT_SIG_LESS_EQUAL | SLJIT_32, SLJIT_R0,
                                 0, SLJIT_IMM, 0);
        break;
      case OP_BGTZ:
        jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
        j_taken = sljit_emit_cmp(c, SLJIT_SIG_GREATER | SLJIT_32, SLJIT_R0, 0,
                                 SLJIT_IMM, 0);
        break;
      case OP_REGIMM:
        if (rt == RT_BLTZ) {
          jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
          j_taken = sljit_emit_cmp(c, SLJIT_SIG_LESS | SLJIT_32, SLJIT_R0, 0,
                                   SLJIT_IMM, 0);
        } else if (rt == RT_BGEZ) {
          jit_load_gpr(c, SLJIT_R0, extract_rs(instr));
          j_taken = sljit_emit_cmp(c, SLJIT_SIG_GREATER_EQUAL | SLJIT_32,
                                   SLJIT_R0, 0, SLJIT_IMM, 0);
        } else {
          /* unknown REGIMM: NOP (no branch), treat as straight-line */
          is_cond = false;
        }
        break;
      default:
        is_cond = false;
        break;
      }

      if (!is_cond) {
        /* Not actually a branch (unknown REGIMM): skip, continue linearly. */
        pc += 4;
        continue;
      }

      /* Layout: taken -> TAKEN label; not-taken -> fall through to pc+8. */
      /* NOT_TAKEN fall-through: delay slot then jump to pc+8. */
      decode_delay_slot(ctx, ds);
      /* Both arms execute the SAME number of guest instructions
       * (straight-line prefix + branch + one delay slot).  Freeze that
       * count now: the not-taken recursion below advances insn_count by
       * everything it translates, and a deferred difference on the taken
       * arm would silently include that pollution. */
      sljit_sw arm_path = (sljit_sw)(ctx->insn_count - bb_start);
      func_emit_jump_to(f, pc + 8, arm_path);

      /* TAKEN body: */
      struct sljit_label *l_taken = sljit_emit_label(c);
      sljit_set_label(j_taken, l_taken);
      decode_delay_slot(ctx, ds);
      /* Delay slot statically occurs in both arms; undo the double count
       * so the static accumulator stays exact (see jit_emit_edge_count). */
      ctx->insn_count--;
      func_emit_jump_to(f, target, arm_path);
      return;
    }

    /* ── Unconditional jump (J/JAL) with delay slot ── */
    if (opcode == OP_J || opcode == OP_JAL) {
      uint32_t target = ((pc + 4) & 0xF0000000) | (extract_target(instr) << 2);
      uint32_t ds = mem_read_32(pc + 4);

      /* The jump instruction itself (decode_delay_slot below only
       * counts the delay slot). */
      ctx->insn_count++;

      if (opcode == OP_JAL) {
        sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG),
                       (sljit_sw)(OFF_GPR + 31 * 4), SLJIT_IMM,
                       (sljit_sw)(pc + 8));
      }
      decode_delay_slot(ctx, ds);

      /* Edge count for this (single-arm) jump: straight-line prefix +
       * jal + one delay slot, frozen before any recursion. */
      sljit_sw arm_path = (sljit_sw)(ctx->insn_count - bb_start);

      /* j/jal target: if inside this function, jump in; else external exit. */
      if (target >= f->func_entry && target < f->func_end)
        func_emit_jump_to(f, target, arm_path);
      else
        func_emit_epilogue(f, target, arm_path);
      return;
    }

    /* ── SPECIAL (R-type) ── */
    if (opcode == OP_SPECIAL) {
      uint32_t funct = extract_funct(instr);

      if (funct == FUN_JR || funct == FUN_JALR) {
        /* Indirect jump / tail call: target unknown at translate time. */
        uint32_t rs = extract_rs(instr);
        uint32_t rd = extract_rd(instr);
        uint32_t ds = mem_read_32(pc + 4);

        /* The jr/jalr instruction itself. */
        ctx->insn_count++;

        if (funct == FUN_JALR) {
          sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(BASE_REG),
                         (sljit_sw)(OFF_GPR + rd * 4), SLJIT_IMM,
                         (sljit_sw)(pc + 8));
        }
        decode_delay_slot(ctx, ds);

        jit_reg_binding_store_all(c);
        jit_load_gpr(c, SLJIT_R0, rs);
        sljit_emit_op1(c, SLJIT_MOV32, SLJIT_MEM1(NEXT_REG), 0, SLJIT_R0, 0);
        jit_emit_edge_count(ctx, (sljit_sw)(ctx->insn_count - bb_start));
        sljit_emit_return_void(c);
        return;
      }

      if (funct == FUN_SYSCALL) {
        /* No delay slot; inline the icall and continue linearly. */
        uint32_t nr = extract_sa(instr);
        uint32_t rd = extract_rd(instr);
        sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)nr);
        sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)rd);
        jit_load_gpr(c, SLJIT_R2, 4);
        jit_load_gpr(c, SLJIT_R3, 5);
        sljit_emit_icall(c, SLJIT_CALL, SLJIT_ARGS4(W, W, W, W, W), SLJIT_IMM,
                         (sljit_sw)(uintptr_t)&jit_syscall);
        jit_store_gpr(c, 2, SLJIT_R0);
        ctx->insn_count++;
        pc += 4;
        continue;
      }

      if (funct == FUN_BREAK) {
        uint32_t code = extract_break_code(instr);
        if (code == 0x3ff) {
          /* The BREAK instruction itself; its exit edge is accounted by
           * func_emit_epilogue below. */
          ctx->insn_count++;
          jit_load_gpr(c, SLJIT_R0, 4);
          sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_IMM,
                         (sljit_sw)code);
          sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R0, 0);
          sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R0, 0, SLJIT_R1, 0);
          sljit_emit_op1(c, SLJIT_MOV32, SLJIT_R1, 0, SLJIT_R2, 0);
          sljit_emit_icall(c, SLJIT_CALL, SLJIT_ARGS2(W, W, W), SLJIT_IMM,
                           (sljit_sw)(uintptr_t)&jit_break);
          /* BREAK has no delay slot. */
          func_emit_epilogue(f, pc + 4, (sljit_sw)(ctx->insn_count - bb_start));
          return;
        }
        /* non-0x3ff break: treat as NOP, continue */
        ctx->insn_count++;
        pc += 4;
        continue;
      }

      /* Ordinary R-type instruction.  Everything except JR/JALR/SYSCALL/
       * BREAK is straight-line lowering (handled above), so this matches
       * what decode_straight_line lowers; an unknown funct is silently treated
       * as a NOP but still occupies its dispatch slot, hence the explicit count
       * (same house style as the SYSCALL/BREAK cases above). */
      ctx->insn_count++;
      decode_straight_line(ctx, instr);
      pc += 4;
      continue;
    }

    /* ── Straight-line instruction (decode_and_emit advances insn_count) ── */
    uint32_t next_pc;
    bool ended;
    decode_and_emit(ctx, instr, pc, &next_pc, &ended);
    (void)ended;
    (void)next_pc;
    pc += 4;
  }
}

/* Emit an intra-function jump target, translating the block on demand.
 *
 * This IS a control-flow edge of the current BB (a taken branch arm, j/jal
 * into the function, ...), so the caller passes that edge's static
 * instruction count in @p path_count.  The external branch is routed to
 * func_emit_epilogue with the same count — do not double count. */
static void func_emit_jump_to(func_ctx_t *f, uint32_t target_pc,
                              sljit_sw path_count) {
  if (target_pc >= f->func_entry && target_pc < f->func_end) {
    jit_emit_edge_count(&f->base, path_count);
    jit_translate_function_block(f, target_pc);
  } else {
    func_emit_epilogue(f, target_pc, path_count);
  }
}

/* Translate a whole function starting at `entry`, using a single compiler so
 * the callee-saved registers are set up once and intra-function control flow
 * is direct `jmp label`.  Returns the entry code pointer or NULL. */
static void *jit_translate_function(uint32_t entry) {
  if (!jit_is_func_entry(entry))
    return NULL;

  struct sljit_compiler *c = sljit_create_compiler(NULL);
  if (!c)
    return NULL;

  func_ctx_t f;
  f.func_entry = entry;
  f.func_end = jit_next_func_entry(entry);
  f.bbs = NULL;
  f.bb_count = 0;
  f.bb_cap = 0;
  f.base.compiler = c;
  f.base.insn_count = 0;

  /* Function-level translation does not use per-block link metadata. */
  f.base.blk = NULL;
  f.base.blk_info = NULL;

  if (sljit_emit_enter(c, 0, 0, 4, 3 + JIT_REG_BINDING_COUNT, 0) !=
      SLJIT_SUCCESS)
    goto fail;

  sljit_emit_op1(c, SLJIT_MOV_P, BASE_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)&g_jit_cpu);
  sljit_emit_op1(c, SLJIT_MOV_P, NEXT_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)&g_jit_next_pc);
  sljit_emit_op1(c, SLJIT_MOV_P, MEM_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)g_emu_memory);

  jit_reg_binding_load_all(c);

  jit_translate_function_block(&f, entry);

  void *code = sljit_generate_code(c, 0, NULL);
  if (!code)
    goto fail;

  sljit_free_compiler(c);
  free(f.bbs);
  jit_direct_table_lookup(entry)->code = code;
  return code;

fail:
  sljit_free_compiler(c);
  free(f.bbs);
  return NULL;
}

#endif

/* ==================================================================== */
/*  Block translator                                                     */
/* ==================================================================== */

static jit_block_t *jit_translate_block(uint32_t start_pc) {
  struct sljit_compiler *c = sljit_create_compiler(NULL);
  if (!c)
    return NULL;

  /* Resolve the block slot up-front so jit_emit_link_exit can record exits. */
  jit_block_t *blk = jit_direct_table_lookup(start_pc);
  jit_block_info_t *blk_info = jit_direct_table_lookup_info(start_pc);

  jit_ctx_t ctx;
  ctx.compiler = c;
  ctx.insn_count = 0;
  ctx.blk = blk;
  ctx.blk_info = blk_info;
  blk_info->exit_count = 0;

  /* 4 scratch registers (R0..R3, used for guest GPR loads and call args)
   * and 3+N saved registers (S0=BASE_REG, S1=NEXT_REG, S2=MEM_REG, plus N
   * for bound guest GPRs). */
  if (sljit_emit_enter(c, 0, 0, 4, 3 + JIT_REG_BINDING_COUNT, 0) !=
      SLJIT_SUCCESS)
    goto fail;

  sljit_emit_op1(c, SLJIT_MOV_P, BASE_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)&g_jit_cpu);
  sljit_emit_op1(c, SLJIT_MOV_P, NEXT_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)&g_jit_next_pc);
  sljit_emit_op1(c, SLJIT_MOV_P, MEM_REG, 0, SLJIT_IMM,
                 (sljit_sw)(uintptr_t)g_emu_memory);

  jit_reg_binding_load_all(c);

  uint32_t pc = start_pc;

  for (;;) {
    uint32_t instr = mem_read_32(pc);
    uint32_t next_pc;
    bool ended;
    decode_and_emit(&ctx, instr, pc, &next_pc, &ended);

    if (ended)
      break;

    pc = next_pc;
    if (ctx.insn_count >= 4096) {
      jit_emit_exit(&ctx, pc);
      break;
    }
  }

  void *code = sljit_generate_code(c, 0, NULL);
  if (!code) {
    sljit_free_compiler(c);
    return NULL;
  }

  /* Capture the executable offset and each exit's jump encoding address
   * before freeing the compiler (needed for sljit_set_jump_addr). */
  blk->code = code;
  blk_info->executable_offset = sljit_get_executable_offset(c);
  for (uint32_t i = 0; i < blk_info->exit_count; i++)
    blk_info->exits[i].jump_addr = sljit_get_jump_addr(blk_info->exits[i].jump);

  sljit_free_compiler(c);

  /* First-pass resolution: chain to already-translated successors, or point
   * at the resolve stub and register a backpatch for the rest. */
  for (uint32_t i = 0; i < blk_info->exit_count; i++)
    jit_link_resolve_exit(blk, blk_info, i);

  return blk;

fail:
  sljit_free_compiler(c);
  return NULL;
}

/* ==================================================================== */
/*  Main execution loop                                                 */
/* ==================================================================== */

static void cpu_jit_execute(void) {
  jit_reset_count();
  jit_reset_exit_code();

  jit_reg_map_init();

  /* The guest image may have changed since the previous run, so the cached
   * translated blocks (and any pending native code) must be discarded. */
  uint32_t image_size, text_end, text_base;
  if (!memory_get_image_info(&image_size, &text_base, &text_end, 0)) {
    text_base = 0;
    text_end = image_size;
  }

  jit_direct_table_init(text_end);

  memset(&g_jit_cpu, 0, sizeof(g_jit_cpu));
  g_jit_cpu.GPR[0] = 0;

  g_jit_next_pc = g_jit_cpu.PC;

  /* Build the function-entry set by scanning .text for `jal` targets (plus
   * the boot entry).  This is purely static and stays valid for the run. */
  uint32_t initial_pc = g_jit_next_pc;
  jit_func_entries_init(text_base, text_end, initial_pc);

  g_cpu_running = true;

  while (g_cpu_running) {
    uint32_t pc = g_jit_next_pc;

    jit_block_t *blk = jit_direct_table_lookup(pc);
    if (blk->code == NULL) {
      /* Prefer whole-function translation for known function entries; fall
       * back to block-level for every other (indirect) landing point. */
      if (!jit_translate_function(pc)) {
        jit_translate_block(pc);
        if (!blk->code) {
          host_stop_running();
          g_cpu_running = false;
          fprintf(stderr, "\n\n!! TCMIPS EMULATOR CRASH, file=%s, line=%u !!\n",
                  __FILE__, __LINE__);
          exit(TCM_EMULATOR_EXIT_INTERNAL);
        }
      }

      /* New block now exists: retarget any pending link-exits that were
       * waiting on this guest PC, so future control flow jumps straight
       * in. */
      jit_backpatch_resolve(pc);
    }

    ((void (*)(void))blk->code)();
  }

  jit_direct_table_clear();
}

/* ==================================================================== */
/*  vtable instance                                                     */
/* ==================================================================== */

const cpu_interpreter_t g_interp_jit = {
    .name = "JIT",
    .get_status = jit_get_status,
    .execute = cpu_jit_execute,
    .get_count = jit_get_count,
    .reset_count = jit_reset_count,
    .get_exit_code = jit_get_exit_code,
};
