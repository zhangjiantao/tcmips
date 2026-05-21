//
// Created by zhangjiantao on 26-5-7.
//

#include "setjmp.h"

#include <stdint.h>

#define _J_S0 0
#define _J_S1 1
#define _J_S2 2
#define _J_S3 3
#define _J_S4 4
#define _J_S5 5
#define _J_S6 6
#define _J_S7 7
#define _J_SP 8
#define _J_FP 9
#define _J_RA 10

__attribute__((returns_twice, noinline)) int
setjmp(jmp_buf env)
{
    __asm__ __volatile__( //
        "sw $s0, %0\n"
        "sw $s1, %1\n"
        "sw $s2, %2\n"
        "sw $s3, %3\n"
        "sw $s4, %4\n"
        "sw $s5, %5\n"
        "sw $s6, %6\n"
        "sw $s7, %7\n"
        "sw $sp, %8\n"
        "sw $fp, %9\n"
        "sw $ra, %10\n"
        :
        : "m"(env[_J_S0]), "m"(env[_J_S1]), "m"(env[_J_S2]), "m"(env[_J_S3]), "m"(env[_J_S4]),
          "m"(env[_J_S5]), "m"(env[_J_S6]), "m"(env[_J_S7]), "m"(env[_J_SP]), "m"(env[_J_FP]),
          "m"(env[_J_RA])
        : "memory");
    return 0;
}

__attribute__((noreturn, noinline)) void
longjmp(jmp_buf env, int val)
{
    // C 标准规定：如果 val 为 0，实际返回 1
    if (val == 0)
        val = 1;

    __asm__ __volatile__( //
        ".set push\n"
        ".set noreorder\n"
        "lw $s0, %1\n"
        "lw $s1, %2\n"
        "lw $s2, %3\n"
        "lw $s3, %4\n"
        "lw $s4, %5\n"
        "lw $s5, %6\n"
        "lw $s6, %7\n"
        "lw $s7, %8\n"
        "lw $sp, %9\n"
        "lw $fp, %10\n"
        "lw $ra, %11\n"
        "move $v0, %0\n" // 设置返回值
        "jr $ra\n"       // 跳转
        "nop\n"
        ".set pop\n"
        :
        : "r"(val), "m"(env[_J_S0]), "m"(env[_J_S1]), "m"(env[_J_S2]), "m"(env[_J_S3]),
          "m"(env[_J_S4]), "m"(env[_J_S5]), "m"(env[_J_S6]), "m"(env[_J_S7]), "m"(env[_J_SP]),
          "m"(env[_J_FP]), "m"(env[_J_RA])
        : "v0", "memory");

    __builtin_unreachable();
}
