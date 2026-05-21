.set noreorder

.global _start
_start:
    j    main
    nop


.align 4
data: .byte 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
xxxx: .byte 0x11, 0xff, 0x33, 0x44, 0xff, 0x66, 0x77, 0xff
yyyy: .byte 0x11, 0xff, 0xff, 0x44, 0xff, 0xff, 0x77, 0xff
.align 4

main:
    li      $v0, 0x12345678

    la      $t0, data

    lw      $t1, 0($t0)
    li      $a0, 0x44332211
    bne     $t1, $a0, exit_failure

    lw      $t1, 1($t0)
    li      $a0, 0x55443322
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 2($t0)
    li      $a0, 0x66554433
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 3($t0)
    li      $a0, 0x77665544
    bne     $t1, $a0, exit_failure
    nop

    li      $a1, 0xffffffff
    sb      $a1, 1($t0)
    sb      $a1, 4($t0)
    sb      $a1, 7($t0)

    lw      $t1, 0($t0)
    li      $a0, 0x4433ff11
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 1($t0)
    li      $a0, 0xff4433ff
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 2($t0)
    li      $a0, 0x66ff4433
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 3($t0)
    li      $a0, 0x7766ff44
    bne     $t1, $a0, exit_failure
    nop

    lw      $t1, 4($t0)
    li      $a0, 0xff7766ff
    bne     $t1, $a0, exit_failure
    nop

    li      $a0, 0x73776d6d
    li      $a1, 0x73776d6d
    syscall 0x20000 + 0x1400 + 7            # rs = $a0, rt = $a1, shamt = 7
    break
    nop
exit_failure:
    li      $a0, 0x40404040
    li      $a1, 0x40404040
    syscall 0x20000 + 0x1400 + 7            # rs = $a0, rt = $a1, shamt = 7
    break
    nop