.set    noreorder

.global _start
_start:
    j      main
    nop

.align      4
result: .word 0                 # should be 0x00001a6d if all instructions work correctly
.align      4

main:
    li      $sp, 0x400000
    li      $a0, 20
    jal     fibonacci
    nop

    sw      $v0, result($zero)
    li      $v1, 0x00001a6d
    bne     $v0, $v1, exit_failure
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

fibonacci:
    addi    $sp, $sp, -12 
    sw      $ra, 0($sp)
    sw      $s0, 4($sp)
    sw      $s1, 8($sp)

    move    $s0, $a0
    li      $t1, 1
    beq     $s0, $zero, return0
    nop
    beq     $s0, $t1,   return1
    nop

    addi    $a0, $s0, -1
    li		$t2, fibonacci
    jalr    $t2
    nop

    addu    $s1, $zero, $v0         # $s1 = fibonacci(y - 1)
    addi    $a0, $s0, -2
    jal     fibonacci               # $v0 = fibonacci(n - 2)
    nop

    addu    $v0, $v0, $s1               # $v0 = fibonacci(n - 2) + $s1

exitfib:
    lw      $ra, 0($sp)         # read registers from stack
    lw      $s0, 4($sp)
    lw      $s1, 8($sp)
    addi    $sp, $sp, 12        # bring back stack pointer
    jr      $ra
    nop

return1:
    li      $v0,1
    j       exitfib
    nop

return0:     
    li      $v0,0
    j       exitfib
    nop


