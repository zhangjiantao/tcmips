.set    noreorder

.global _start
_start:
    li     $sp, 0x400000
    j      main
    nop

.align      4
cyclecount:         .word 0x88888888
timestamp:          .word 0x88888888
timestamp_milli:    .word 0
timestamp_micro:    .word 0
keyboard_code:      .word 0

.align      4
syscall_rdcycle:
    syscall 0
    sw      $v0, cyclecount
    jr      $ra
    nop

syscall_rdtimestamp:
    syscall 1
    sw      $v0, timestamp
    jr      $ra
    nop

syscall_rdtimestamp_milli:
    syscall 2
    sw      $v0, timestamp_milli
    jr      $ra
    nop

syscall_rdtimestamp_micro:
    syscall 3
    sw      $v0, timestamp_micro
    jr      $ra
    nop

syscall_rdkeyboard:
    syscall 4
    sw      $v0, keyboard_code
    jr      $ra
    nop

syscall_start_playsound:
    syscall 0x20000 + 0x1400 + 0x20 + 5     # rs = $a0, rt = $a1, rd = 1, shamt = 5
    jr      $ra
    nop

syscall_reset_and_playsound:
    syscall 0x20000 + 0x1400 + 0x40 + 5     # rs = $a0, rt = $a1, rd = 2, shamt = 5
    jr      $ra
    nop

syscall_stop_playsound:
    syscall 0x20000 + 0x1400 + 0x60 + 5     # rs = $a0, rt = $a1, rd = 3, shamt = 5
    jr      $ra
    nop

syscall_readfile:
    syscall 0x20000 + 0x1400 + 6            # rs = $a0, rt = $a1, shamt = 6
    jr      $ra
    nop

syscall_segmentdisplay0:
    syscall 0x20000 + 0x1400 + 7            # rs = $a0, rt = $a1, shamt = 7
    jr      $ra
    nop

syscall_segmentdisplay1:
    syscall 0x20000 + 0x1400 + 0x20 + 7            # rs = $a0, rt = $a1, rd = 1, shamt = 7
    jr      $ra
    nop


readfile:
    addi    $sp, $sp, -16
    sw      $ra, 0($sp)
    li      $a0, 0xffffffff
    li      $a1, 0xffffffff
    jal     syscall_readfile
    nop
    move    $t3, $v0
    li      $t1, 0x4000 + 0x780
    li      $a0, 0
readfile_screen_base:
    li      $t0, 0x4000
readfile_loop:
    li      $a1, 0
    jal     syscall_readfile
    nop
    sw      $v0, 0($t0)
    addi    $t0, $t0, 4
    addi    $a0, $a0, 4
    sltiu   $t2, $t0, 0x4000 + 0x780
    beq     $t2, $zero, readfile_screen_base
    nop
    sltu    $t2, $a0, $t3
    beq     $t2, $zero, readfile_exit
    nop
    j       readfile_loop
    nop
readfile_exit:
    lw      $ra, 0($sp)
    addi    $sp, $sp, 16
    jr      $ra
    nop

segment_display0:
    addi    $sp, $sp, -32
    sw      $ra, 0($sp)
    sw      $t0, 4($sp)
    sw      $t1, 8($sp)
    la      $v0, seg_table0
    lw      $t0, timestamp
    li      $a0, 0
    li      $a1, 0
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $a0, 0($v1)
    ror     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1
    ror     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1
    ror     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1
    ror     $a0, $a0, 8

    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $a1, 0($v1)
    ror     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1
    ror     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1
    ror     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1
    ror     $a1, $a1, 8

    jal     syscall_segmentdisplay0
    nop
    lw      $ra, 0($sp)
    lw      $t0, 4($sp)
    lw      $t1, 8($sp)
    addi    $sp, $sp, 32
    jr      $ra
    nop

segment_display1:
    addi    $sp, $sp, -32
    sw      $ra, 0($sp)
    sw      $t0, 4($sp)
    sw      $t1, 8($sp)
    la      $v0, seg_table1
    lw      $t0, cyclecount
    li      $a0, 0
    li      $a1, 0
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $a0, 0($v1)
    sll     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1
    sll     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1
    sll     $a0, $a0, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a0, $a0, $t1

    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $a1, 0($v1)
    sll     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1
    sll     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1
    sll     $a1, $a1, 8
    srl     $t0, $t0, 4
    andi    $t1, $t0, 0x0f
    sll     $t1, $t1, 2
    add     $v1, $v0, $t1
    lw      $t1, 0($v1)
    or      $a1, $a1, $t1

    move    $v0, $a0
    move    $a0, $a1
    move    $a1, $v0
    jal     syscall_segmentdisplay1
    nop
    lw      $ra, 0($sp)
    lw      $t0, 4($sp)
    lw      $t1, 8($sp)
    addi    $sp, $sp, 32
    jr      $ra
    nop

main:
    jal     segment_display0
    nop
    jal     segment_display1
    nop
    jal     readfile
    nop

    li      $t2, 100

clear:
    li      $t0, -1

loop:
    jal     segment_display0
    nop
    jal     segment_display1
    nop
    jal     syscall_rdcycle
    nop
    jal     syscall_rdtimestamp
    nop
    jal     syscall_rdtimestamp_milli
    nop
    jal     syscall_rdtimestamp_micro
    nop
    jal     syscall_rdkeyboard
    nop

    beq     $v0, $zero, clear
    nop
    andi    $t1, $v0, 0x100
    bne     $t1, $zero, clear
    nop

    beq     $v0, $t0, loop
    nop

    move    $t0, $v0
    divu    $v0, $t2
    mfhi    $a0
    li      $a1, 0
    jal     syscall_reset_and_playsound
    nop

    j       loop
    nop

.align      4
seg_table0: .word 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71
seg_table1: .word 0x3f, 0x30, 0x5b, 0x79, 0x74, 0x6d, 0x6f, 0x38, 0x7f, 0x7d, 0x7e, 0x67, 0x0f, 0x73, 0x4f, 0x4e
