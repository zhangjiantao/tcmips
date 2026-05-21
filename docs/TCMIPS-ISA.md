## TCMIPS Instruction Set Specification

## Introduction

This instruction set is based on MIPS32r2 architecture, with privileged instructions, complex fused instructions, hardware exception traps on overflow, and all Likely branch architectural variants removed.

### Special Notes:

* **Memory access:** All multi-byte memory access instructions (`LH`, `LW`, `LHU`, `SH`, `SW`) support unaligned address access natively in hardware via byte-lane multiplexing networks.
* **Trap:** Trap instructions execute as NOPs (No Operation) and alter no processor state.
* **Overflow Exceptions:** Signed arithmetic instructions (`ADD`, `SUB`, `ADDI`) do not trigger hardware overflow exceptions; they wrap around silently and return the lower 32 bits of the result, behaving identically to their unsigned counterparts (`ADDU`, `SUBU`, `ADDIU`).
* **Syscall:** The SYSCALL instruction does not trigger a privilege level switch; returns are handled directly by hardware circuitry within a single instruction cycle.
* **Branch Delay Slot:** Standard MIPS branch delay slot is supported. The instruction immediately following any control transfer instruction (branches and jumps) is executed unconditionally, regardless of whether the branch is taken or not taken.
* **Breakpoint Debugging:** The `BREAK` instruction natively asserts an external hardware signal designed to trigger an in-game interrupt component, interfacing directly with the game's simulation control.


The tables below list all supported instructions classified by functional blocks.

------------------------------

## Instruction Set Reference Tables

## 1. Arithmetic Instructions

| Instruction | Type / Opcode       | fn Field (Dec) | Bitwise Expression | Format | Description / Semantics |
|---|---------------------|---|---|---|---|
| ADD | op == 0 (SPECIAL)   | 32 | ((4 << 3) + 0) | R-Type | Add Word (No Overflow Exception) |
| ADDU | op == 0 (SPECIAL)   | 33 | ((4 << 3) + 1) | R-Type | Add Word Unsigned (No Overflow Exception) |
| SUB | op == 0 (SPECIAL)   | 34 | ((4 << 3) + 2) | R-Type | Subtract Word (No Overflow Exception) |
| SUBU | op == 0 (SPECIAL)   | 35 | ((4 << 3) + 3) | R-Type | Subtract Word Unsigned |
| MULT | op == 0 (SPECIAL)   | 24 | ((3 << 3) + 0) | R-Type | Multiply Word (Signed) |
| MULTU | op == 0 (SPECIAL)   | 25 | ((3 << 3) + 1) | R-Type | Multiply Word Unsigned |
| DIV | op == 0 (SPECIAL)   | 26 | ((3 << 3) + 2) | R-Type | Divide Word (Signed) |
| DIVU | op == 0 (SPECIAL)   | 27 | ((3 << 3) + 3) | R-Type | Divide Word Unsigned |
| MUL | op == 28 (SPECIAL2) | 2 | ((0 << 3) + 2) | R-Type | Multiply Word to GPR |
| ADDI | op == 8             | - | ((1 << 3) + 0) | I-Type | Add Immediate (Signed, No Overflow Exception) |
| ADDIU | op == 9             | - | ((1 << 3) + 1) | I-Type | Add Immediate Unsigned |

## 2. Logical Instructions

| Instruction | Type / Opcode | fn Field (Dec) | Bitwise Expression | Format | Description / Semantics |
|---|---|---|---|---|---|
| AND | op == 0 (SPECIAL) | 36 | ((4 << 3) + 4) | R-Type | Bitwise AND |
| OR | op == 0 (SPECIAL) | 37 | ((4 << 3) + 5) | R-Type | Bitwise OR |
| XOR | op == 0 (SPECIAL) | 38 | ((4 << 3) + 6) | R-Type | Bitwise XOR |
| NOR | op == 0 (SPECIAL) | 39 | ((4 << 3) + 7) | R-Type | Bitwise NOR |
| ANDI | op == 12 | - | ((1 << 3) + 4) | I-Type | Bitwise AND Immediate |
| ORI | op == 13 | - | ((1 << 3) + 5) | I-Type | Bitwise OR Immediate |
| XORI | op == 14 | - | ((1 << 3) + 6) | I-Type | Bitwise XOR Immediate |
| LUI | op == 15 | - | ((1 << 3) + 7) | I-Type | Load Upper Immediate |

## 3. Shift Instructions

| Instruction | Type / Opcode | fn Field (Dec) | Bitwise Expression | Format | Description / Semantics |
|---|---|---|---|---|---|
| SLL | op == 0 (SPECIAL) | 0 | ((0 << 3) + 0) | R-Type | Shift Word Left Logical |
| SRL | op == 0 (SPECIAL) | 2 | ((0 << 3) + 2) | R-Type | Shift Word Right Logical |
| SRA | op == 0 (SPECIAL) | 3 | ((0 << 3) + 3) | R-Type | Shift Word Right Arithmetic |
| SLLV | op == 0 (SPECIAL) | 4 | ((0 << 3) + 4) | R-Type | Shift Word Left Logical Variable |
| SRLV | op == 0 (SPECIAL) | 6 | ((0 << 3) + 6) | R-Type | Shift Word Right Logical Variable |
| SRAV | op == 0 (SPECIAL) | 7 | ((0 << 3) + 7) | R-Type | Shift Word Right Arithmetic Variable |

## 4. Memory Access Instructions

| Instruction | Type / Opcode | Bitwise Expression | Format | Description / Semantics |
|---|---|---|---|---|
| LB | op == 32 | ((4 << 3) + 0) | I-Type | Load Byte |
| LH | op == 33 | ((4 << 3) + 1) | I-Type | Load Halfword (Supports unaligned access) |
| LW | op == 35 | ((4 << 3) + 3) | I-Type | Load Word (Supports unaligned access) |
| LBU | op == 36 | ((4 << 3) + 4) | I-Type | Load Byte Unsigned |
| LHU | op == 37 | ((4 << 3) + 5) | I-Type | Load Halfword Unsigned (Supports unaligned access) |
| SB | op == 40 | ((5 << 3) + 0) | I-Type | Store Byte |
| SH | op == 41 | ((5 << 3) + 1) | I-Type | Store Halfword (Supports unaligned access) |
| SW | op == 43 | ((5 << 3) + 3) | I-Type | Store Word (Supports unaligned access) |

## 5. Control Transfer Instructions

| Instruction | Type / Opcode | Decoding Field | Bitwise Expression | Format | Description / Semantics |
|---|---|---|---|---|---|
| J | op == 2 | - | ((0 << 3) + 2) | J-Type | Jump to Target Address |
| JAL | op == 3 | - | ((0 << 3) + 3) | J-Type | Jump and Link (Procedure Call) |
| BEQ | op == 4 | - | ((0 << 3) + 4) | I-Type | Branch on Equal |
| BNE | op == 5 | - | ((0 << 3) + 5) | I-Type | Branch on Not Equal |
| BLEZ | op == 6 | - | ((0 << 3) + 6) | I-Type | Branch on Less Than or Equal to Zero |
| BGTZ | op == 7 | - | ((0 << 3) + 7) | I-Type | Branch on Greater Than Zero |
| BLTZ | op == 1 (REGIMM) | rt == 0 | ((0 << 3) + 0) | I-Type | Branch on Less Than Zero |
| BGEZ | op == 1 (REGIMM) | rt == 1 | ((0 << 3) + 1) | I-Type | Branch on Greater Than or Equal to Zero |
| JR | op == 0 (SPECIAL) | fn == 8 | ((1 << 3) + 0) | R-Type | Jump Register |
| JALR | op == 0 (SPECIAL) | fn == 9 | ((1 << 3) + 1) | R-Type | Jump and Link Register |

## 6. Move & Bit-Field Instructions

| Instruction | Primary Opcode (op) | fn / sh Field (Dec) | Decoded Field (fn, sh) | Format | Description / Semantics |
|---|---|---|---|---|---|
| MOVZ | 0 (SPECIAL) | fn == 10 | (10, -) | R-Type | Move Conditional on Zero |
| MOVN | 0 (SPECIAL) | fn == 11 | (11, -) | R-Type | Move Conditional on Not Zero |
| MFHI | 0 (SPECIAL) | fn == 16 | (16, -) | R-Type | Move From HI Register |
| MTHI | 0 (SPECIAL) | fn == 17 | (17, -) | R-Type | Move To HI Register |
| MFLO | 0 (SPECIAL) | fn == 18 | (18, -) | R-Type | Move From LO Register |
| MTLO | 0 (SPECIAL) | fn == 19 | (19, -) | R-Type | Move To LO Register |
| EXT | 31 (SPECIAL3) | fn == 0 | (0, -) | R-Type | Extract Bit Field |
| INS | 31 (SPECIAL3) | fn == 4 | (4, -) | R-Type | Insert Bit Field |
| WSBH | 31 (SPECIAL3) | fn == 32, sh == 2 | (32, 2) | R-Type | Word Swap Bytes Within Halfwords |
| SEB | 31 (SPECIAL3) | fn == 32, sh == 16 | (32, 16) | R-Type | Sign-Extend Byte |
| SEH | 31 (SPECIAL3) | fn == 32, sh == 24 | (32, 24) | R-Type | Sign-Extend Halfword |

## 7. Comparison, System & Null Instructions

| Instruction | Primary Opcode (op) | fn Field (Dec) | Bitwise Expression | Format | Description / Semantics |
|---|---|---|---|---|---|
| SLT | 0 (SPECIAL) | 42 | ((5 << 3) + 2) | R-Type | Set on Less Than (Signed) |
| SLTU | 0 (SPECIAL) | 43 | ((5 << 3) + 3) | R-Type | Set on Less Than Unsigned |
| SLTI | 10 | - | ((1 << 3) + 2) | I-Type | Set on Less Than Immediate (Signed) |
| SLTIU | 11 | - | ((1 << 3) + 3) | I-Type | Set on Less Than Immediate Unsigned |
| SYSCALL | 0 (SPECIAL) | 12 | ((1 << 3) + 4) | R-Type | System Call (Handled directly by hardware circuitry within a single instruction cycle) |
| BREAK | 0 (SPECIAL) | 13 | ((1 << 3) + 5) | R-Type | Breakpoint (Triggers an in-game interrupt component) |
| TGE | 0 (SPECIAL) | 48 | ((6 << 3) + 0) | R-Type | Trap Executed as NOP (No Operation) |
| TGEU | 0 (SPECIAL) | 49 | ((6 << 3) + 1) | R-Type | Trap Executed as NOP (No Operation) |
| TLT | 0 (SPECIAL) | 50 | ((6 << 3) + 2) | R-Type | Trap Executed as NOP (No Operation) |
| TLTU | 0 (SPECIAL) | 51 | ((6 << 3) + 3) | R-Type | Trap Executed as NOP (No Operation) |
| TEQ | 0 (SPECIAL) | 52 | ((6 << 3) + 4) | R-Type | Trap Executed as NOP (No Operation) |
| TNE | 0 (SPECIAL) | 54 | ((6 << 3) + 6) | R-Type | Trap Executed as NOP (No Operation) |


