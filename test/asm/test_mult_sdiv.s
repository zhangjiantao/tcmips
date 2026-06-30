.set noreorder
.global _start
_start:
    j       init
    nop

.align      4
a: .word 0
b: .word 0
c: .word 0
d: .word 0
result: .word 0 # should be 0x000806c6 if all instructions work correctly
.align      4

init:
    li      $t0, 0x01234567
    li      $t1, 0x89abcdef
    li      $t2, 0xa5a5a5a5
    li      $t3, 20

loop:
    mult    $t0, $t1
    mflo    $t0
    ror     $t0, $t0, 7
    xor     $t0, $t0, $t2
    mfhi    $t1
    srl     $t1, $t1, 21
    xori    $t1, $t1, 0xa5
    ori     $t1, $t1, 0x80
    sw      $t0, a($zero)
    sw      $t1, b($zero)
    div     $t0, $t1
    mflo    $t0
    mfhi    $t1
    sw      $t0, c($zero)
    sw      $t1, d($zero)
    multu   $t0, $t1
    mflo    $t0
    ror     $t0, $t0, 7
    xor     $t0, $t0, $t2
    mfhi    $t1
    srl     $t1, $t1, 21
    xori    $t1, $t1, 0xa5
    ori     $t1, $t1, 0x80
    sw      $t0, a($zero)
    sw      $t1, b($zero)
    divu    $t0, $t1
    mflo    $t0
    mfhi    $t1
    sw      $t0, c($zero)
    sw      $t1, d($zero)
    addi    $t3, $t3, -1
    bne     $t3, $zero, loop
    nop
    xor     $t0, $t0, $t1
    sw      $t0, result($zero)
    li      $v1, 0x000806c6
    bne     $t0, $v1, exit_failure
    nop
    li      $a0, 0x73776d6d
    li      $a1, 0x73776d6d
    syscall 0x20000 + 0x1400 + 2            # rs = $a0, rt = $a1, shamt = 7
    break
    nop
exit_failure:
    li      $a0, 0x40404040
    li      $a1, 0x40404040
    syscall 0x20000 + 0x1400 + 2            # rs = $a0, rt = $a1, shamt = 7
    break
    nop