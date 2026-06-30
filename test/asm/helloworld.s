    .set noreorder

    .global _start
_start:
    lw   $v0, console_buffer_base
    lw   $v1, console_buffer_size
    add  $v1, $v0, $v1

loop:
    la   $t0, str
    lw   $a0, 0($t0)
    sw   $a0, 0($v0)
    lw   $a0, 4($t0)
    sw   $a0, 4($v0)
    lw   $a0, 8($t0)
    sw   $a0, 8($v0)
    addi $v0, $v0, 12

    sltu $t2, $v0, $v1
    bne  $t2, $0, loop
    nop

exit:
    break
    nop

    .align 4
console_buffer_base:             
    .word 0x10000
console_buffer_size:
    .word 0x780*4                     # size of console buffer in bytes

    .align 4
str:
    .ascii "HelloWorld! "
