
.set noreorder
.global _start
_start:
    j       init
    nop

.align      4
res: .word 0             # should be 0xcdc5d258 if all instructions work correctly
.align      4

init:
    li   	$a0, 0xfefefefe
    li   	$a1, 0xabababab
    li   	$a2, 0x11223344
    li   	$a3, 0x55667788
    li   	$t0, 0xaabbccdd
    li   	$t1, 0xcdcdcdcd
    ins     $t1, $a0, 3, 7
    xor     $t2, $t2, $t1
    ins     $t1, $a1, 4, 8
    xor     $t2, $t2, $t1
    ins     $t1, $a2, 5, 9
    xor     $t2, $t2, $t1
    ins     $t1, $a3, 6, 10
    xor     $t2, $t2, $t1
    ins     $t1, $t0, 7, 11
    xor     $t2, $t2, $t1

	ext  	$t2, $a0, 0, 16
	xor     $t2, $t2, $t1
	ext  	$t2, $a1, 1, 17
	xor     $t2, $t2, $t1
	ext  	$t2, $a2, 2, 18
	xor     $t2, $t2, $t1
	ext  	$t2, $a3, 3, 19
	xor     $t2, $t2, $t1
	ext  	$t2, $t0, 4, 20
    xor     $t2, $t2, $t1

    sw      $t2, res($zero)
    li      $v0, 0xcdc5d258
    bne     $t2, $v0, exit_failure
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



