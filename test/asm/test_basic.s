.set noreorder
.global _start
_start:
    j       init
    nop

.align      4
result: .word 0             # should be 0xa05977cd if all instructions work correctly
.align      4

init:
    li      $s0, 0x01234567
    li      $s1, 0x89abcdef
    li      $s2, 0xa5a5a5a5
    li      $s3, 32
    li      $s4, 0

loop:
    move    $t0, $s0
    move    $t1, $s1
    move    $t2, $s2
	
    and     $t0, $t0, $t1
    or      $t0, $t0, $t2
    xor     $t0, $t0, $t1
    not     $t0, $t0
    nor     $t0, $t0, $t1
    andi    $t0, $t0, 0xfefe
    ori     $t0, $t0, 0x1010
    xori    $t0, $t0, 0xa5a5
    addu    $t0, $t0, $t1
    subu    $t0, $t0, $t2
    sll     $t0, $t0, 5
    srl     $t0, $t0, 3
    sra     $t0, $t0, 2
    ror     $t0, $t0, 7
    ror     $t0, $t0, $t1
    rol     $t0, $t0, 3
    rol     $t0, $t0, $t1

    xor     $s4, $s4, $t0
    xor     $s4, $s4, $t1
    xor     $s4, $s4, $t2
    xor     $s0, $s0, $t0
    xor     $s1, $s1, $t0
    xor     $s2, $s2, $t0
    rol     $s0, $s0, 1
    rol     $s1, $s1, 1
    rol     $s2, $s2, 1

    addi    $s3, $s3, -1
    bne     $s3, $zero, loop
    nop

    sw      $s4, result
    li      $v0, 0xa05977cd
    bne     $s4, $v0, exit_failure
    nop
    li      $a0, 0x73776d6d
    li      $a1, 0x73776d6d
    syscall 0x20000 + 0x1400 + 2            # rs = $a0, rt = $a1, shamt = 2
    break
    nop

exit_failure:
    li      $a0, 0x40404040
    li      $a1, 0x40404040
    syscall 0x20000 + 0x1400 + 2            # rs = $a0, rt = $a1, shamt = 2
    break
    nop
