//
// Created by zhangjiantao on 2026/6/8.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum {
  FUN2_MUL = 0x02,
} Special2Funct;

typedef enum { FUN3_BEXT = 0x20 } Special3Funct;

typedef enum { SA_WSBH = 0x02, SA_SEB = 0x10, SA_SEH = 0x18 } Special3Sa;

typedef enum { RT_BLTZ = 0x00, RT_BGEZ = 0x01 } RegImmRt;

typedef struct {
  uint32_t GPR[32];
  uint32_t PC;
  uint32_t PC_next;
  uint32_t HI;
  uint32_t LO;
  int is_delay_slot;
} MIPS_CPU;
#define MEM_SIZE (1024 * 1024 * 64)
uint8_t memory[MEM_SIZE];

uint32_t sign_extend_16(uint16_t val) { return (uint32_t)((int16_t)val); }
uint32_t zero_extend_16(uint16_t val) { return (uint32_t)val; }
uint32_t sign_extend_8(uint8_t val) { return (uint32_t)((int8_t)val); }
uint32_t zero_extend_8(uint8_t val) { return (uint32_t)val; }

uint32_t read_mem(uint32_t addr, int bytes) {
  if (addr + bytes > MEM_SIZE)
    return 0;
  uint32_t res = 0;
  for (int i = 0; i < bytes; i++) {
    res |= (uint32_t)memory[addr + i] << (i * 8);
  }
  return res;
}

void write_mem(uint32_t addr, int bytes, uint32_t data) {
  if (addr + bytes > MEM_SIZE)
    return;
  for (int i = 0; i < bytes; i++) {
    memory[addr + i] = (data >> (i * 8)) & 0xFF;
  }
}

void execute_instruction(MIPS_CPU *cpu, uint32_t instr) {
  Opcode opcode = (Opcode)((instr >> 26) & 0x3F);
  uint32_t rs = (instr >> 21) & 0x1F;
  uint32_t rt = (instr >> 16) & 0x1F;
  uint32_t rd = (instr >> 11) & 0x1F;
  uint32_t sa = (instr >> 6) & 0x1F;
  uint32_t funct = instr & 0x3F;
  uint16_t imm = instr & 0xFFFF;
  uint32_t target = instr & 0x3FFFFFF;

  uint32_t val_rs = cpu->GPR[rs];
  uint32_t val_rt = cpu->GPR[rt];

  int take_branch = 0;
  uint32_t branch_target = cpu->PC + 4;
  uint32_t next_pc_default = cpu->PC + 4;

  switch (opcode) {
  case OP_LB:
    cpu->GPR[rt] = sign_extend_8(read_mem(val_rs + sign_extend_16(imm), 1));
    break;
  case OP_LH:
    cpu->GPR[rt] = sign_extend_16(read_mem(val_rs + sign_extend_16(imm), 2));
    break;
  case OP_LW:
    cpu->GPR[rt] = read_mem(val_rs + sign_extend_16(imm), 4);
    break;
  case OP_LBU:
    cpu->GPR[rt] = zero_extend_8(read_mem(val_rs + sign_extend_16(imm), 1));
    break;
  case OP_LHU:
    cpu->GPR[rt] = zero_extend_16(read_mem(val_rs + sign_extend_16(imm), 2));
    break;
  case OP_SB:
    write_mem(val_rs + sign_extend_16(imm), 1, val_rt & 0xFF);
    break;
  case OP_SH:
    write_mem(val_rs + sign_extend_16(imm), 2, val_rt & 0xFFFF);
    break;
  case OP_SW:
    write_mem(val_rs + sign_extend_16(imm), 4, val_rt);
    break;
  case OP_ADDIU:
    cpu->GPR[rt] = val_rs + sign_extend_16(imm);
    break;
  case OP_SLTI:
    cpu->GPR[rt] = ((int32_t)val_rs < (int32_t)sign_extend_16(imm)) ? 1 : 0;
    break;
  case OP_SLTIU:
    cpu->GPR[rt] = ((uint32_t)val_rs < (uint32_t)sign_extend_16(imm)) ? 1 : 0;
    break;
  case OP_ANDI:
    cpu->GPR[rt] = val_rs & zero_extend_16(imm);
    break;
  case OP_ORI:
    cpu->GPR[rt] = val_rs | zero_extend_16(imm);
    break;
  case OP_XORI:
    cpu->GPR[rt] = val_rs ^ zero_extend_16(imm);
    break;
  case OP_LUI:
    cpu->GPR[rt] = (uint32_t)imm << 16;
    break;
  case OP_J:
    take_branch = 1;
    branch_target = ((cpu->PC + 4) & 0xF0000000) | (target << 2);
    break;
  case OP_JAL:
    cpu->GPR[31] = cpu->PC + 8;
    take_branch = 1;
    branch_target = ((cpu->PC + 4) & 0xF0000000) | (target << 2);
    break;
  case OP_BEQ:
    take_branch = (val_rs == val_rt);
    branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                : (cpu->PC + 8);
    take_branch = 1;
    break;
  case OP_BNE:
    take_branch = (val_rs != val_rt);
    branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                : (cpu->PC + 8);
    take_branch = 1;
    break;
  case OP_BLEZ:
    take_branch = ((int32_t)val_rs <= 0);
    branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                : (cpu->PC + 8);
    take_branch = 1;
    break;
  case OP_BGTZ:
    take_branch = ((int32_t)val_rs > 0);
    branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                : (cpu->PC + 8);
    take_branch = 1;
    break;

  case OP_REGIMM: {
    switch ((RegImmRt)rt) {
    case RT_BLTZ:
      take_branch = ((int32_t)val_rs < 0);
      branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                  : (cpu->PC + 8);
      take_branch = 1;
      break;
    case RT_BGEZ:
      take_branch = ((int32_t)val_rs >= 0);
      branch_target = take_branch ? (cpu->PC + 4 + (sign_extend_16(imm) << 2))
                                  : (cpu->PC + 8);
      take_branch = 1;
      break;
    }
    break;
  }

  case OP_SPECIAL2: {
    switch ((Special2Funct)funct) {
    case FUN2_MUL: {
      uint64_t res = (int64_t)(int32_t)val_rs * (int64_t)(int32_t)val_rt;
      cpu->GPR[rd] = (uint32_t)(res & 0xFFFFFFFF);
      break;
    }
    }
    break;
  }

  case OP_SPECIAL3: {
    if ((Special3Funct)funct == FUN3_BEXT) {
      switch ((Special3Sa)sa) {
      case SA_WSBH:
        cpu->GPR[rd] =
            ((val_rt & 0x00FF0000) >> 8) | ((val_rt & 0xFF000000) >> 8) |
            ((val_rt & 0x000000FF) << 8) | ((val_rt & 0x0000FF00) << 8);
        break;
      case SA_SEB:
        cpu->GPR[rd] = sign_extend_8(val_rt & 0xFF);
        break;
      case SA_SEH:
        cpu->GPR[rd] = sign_extend_16(val_rt & 0xFFFF);
        break;
      }
    }
    break;
  }

  case OP_SPECIAL: {
    switch ((SpecialFunct)funct) {
    case FUN_JR:
      take_branch = 1;
      branch_target = val_rs;
      break;
    case FUN_JALR:
      cpu->GPR[rd] = cpu->PC + 8;
      take_branch = 1;
      branch_target = val_rs;
      break;
    case FUN_SLL:
      cpu->GPR[rd] = val_rt << sa;
      break;
    case FUN_SRL:
      cpu->GPR[rd] = val_rt >> sa;
      break;
    case FUN_SRA:
      cpu->GPR[rd] = (uint32_t)((int32_t)val_rt >> sa);
      break;
    case FUN_SLLV:
      cpu->GPR[rd] = val_rt << (val_rs & 0x1F);
      break;
    case FUN_SRLV:
      cpu->GPR[rd] = val_rt >> (val_rs & 0x1F);
      break;
    case FUN_SRAV:
      cpu->GPR[rd] = (uint32_t)((int32_t)val_rt >> (val_rs & 0x1F));
      break;
    case FUN_MOVZ:
      if (val_rt == 0)
        cpu->GPR[rd] = val_rs;
      break;
    case FUN_MOVN:
      if (val_rt != 0)
        cpu->GPR[rd] = val_rs;
      break;
    case FUN_SYSCALL:
      printf("Syscall PC: 0x%08X\n", cpu->PC);
      break;
    case FUN_BREAK:
      printf("Breakpoint PC: 0x%08X\n", cpu->PC);
      break;
    case FUN_ADDU:
      cpu->GPR[rd] = val_rs + val_rt;
      break;
    case FUN_SUBU:
      cpu->GPR[rd] = val_rs - val_rt;
      break;
    case FUN_AND:
      cpu->GPR[rd] = val_rs & val_rt;
      break;
    case FUN_OR:
      cpu->GPR[rd] = val_rs | val_rt;
      break;
    case FUN_XOR:
      cpu->GPR[rd] = val_rs ^ val_rt;
      break;
    case FUN_NOR:
      cpu->GPR[rd] = ~(val_rs | val_rt);
      break;
    case FUN_SLT:
      cpu->GPR[rd] = ((int32_t)val_rs < (int32_t)val_rt) ? 1 : 0;
      break;
    case FUN_SLTU:
      cpu->GPR[rd] = ((uint32_t)val_rs < (uint32_t)val_rt) ? 1 : 0;
      break;
    case FUN_MFHI:
      cpu->GPR[rd] = cpu->HI;
      break;
    case FUN_MTHI:
      cpu->HI = val_rs;
      break;
    case FUN_MFLO:
      cpu->GPR[rd] = cpu->LO;
      break;
    case FUN_MTLO:
      cpu->LO = val_rs;
      break;
    case FUN_MULT: {
      int64_t res = (int64_t)(int32_t)val_rs * (int64_t)(int32_t)val_rt;
      cpu->LO = (uint32_t)(res & 0xFFFFFFFF);
      cpu->HI = (uint32_t)((res >> 32) & 0xFFFFFFFF);
      break;
    }
    case FUN_MULTU: {
      uint64_t res = (uint64_t)val_rs * (uint64_t)val_rt;
      cpu->LO = (uint32_t)(res & 0xFFFFFFFF);
      cpu->HI = (uint32_t)((res >> 32) & 0xFFFFFFFF);
      break;
    }
    case FUN_DIV:
      if (val_rt != 0) {
        cpu->LO = (uint32_t)((int32_t)val_rs / (int32_t)val_rt);
        cpu->HI = (uint32_t)((int32_t)val_rs % (int32_t)val_rt);
      }
      break;
    case FUN_DIVU:
      if (val_rt != 0) {
        cpu->LO = val_rs / val_rt;
        cpu->HI = val_rs % val_rt;
      }
      break;
    }
    break;
  }
  }

  cpu->GPR[0] = 0;

  if (cpu->is_delay_slot) {
    cpu->PC = cpu->PC_next;
    cpu->is_delay_slot = 0;
  } else {
    if (take_branch) {
      cpu->PC_next = branch_target;
      cpu->PC = next_pc_default;
      cpu->is_delay_slot = 1;
    } else {
      cpu->PC = next_pc_default;
    }
  }
}

static const char *reg_names[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2",
    "t3",   "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5",
    "s6",   "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

void dump_cpu(MIPS_CPU *cpu) {
  printf("========== CPU State ==========\n");
  printf("PC   : 0x%08X    PC_next : 0x%08X\n", cpu->PC, cpu->PC_next);
  printf("HI   : 0x%08X    LO      : 0x%08X\n", cpu->HI, cpu->LO);
  printf("DelaySlot: %s\n", cpu->is_delay_slot ? "Yes" : "No");
  printf("-------------------------------\n");
  for (int i = 0; i < 32; i += 4) {
    printf("$%-4s(r%02d): 0x%08X    $%-4s(r%02d): 0x%08X    $%-4s(r%02d): "
           "0x%08X    $%-4s(r%02d): 0x%08X\n",
           reg_names[i], i, cpu->GPR[i], reg_names[i + 1], i + 1,
           cpu->GPR[i + 1], reg_names[i + 2], i + 2, cpu->GPR[i + 2],
           reg_names[i + 3], i + 3, cpu->GPR[i + 3]);
  }
  printf("===============================\n\n");
}

int main() {
  MIPS_CPU cpu;
  memset(&cpu, 0, sizeof(MIPS_CPU));
  memset(memory, 0, MEM_SIZE);
  write_mem(0x00, 4, 0x24010005);
  write_mem(0x04, 4, 0x08000003);
  write_mem(0x08, 4, 0x2402000A);
  write_mem(0x0C, 4, 0x00221821);
  cpu.PC = 0x00;
  cpu.GPR[29] = MEM_SIZE;

  printf("--- start emu ---\n");
  for (int i = 0; i < 4; i++) {
    uint32_t current_instr = read_mem(cpu.PC, 4);
    execute_instruction(&cpu, current_instr);
    dump_cpu(&cpu);
  }
  return 0;
}
