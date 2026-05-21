//
// Created by zhangjiantao on 2026/5/19.
//

/*
.text:000076A0                 .globl main
.text:000076A0 main:                                    # CODE XREF: cstart+A0↑p
.text:000076A0                 li      $at, 0x3E8
.text:000076A4                 div     $a0, $at
.text:000076A8                 teq     $at, $zero  #7
.text:000076AC                 jr      $ra
.text:000076B0                 mflo    $v0
*/

int main(int argc, char **argv) { return argc / 1000; }