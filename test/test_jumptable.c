#include <stdio.h>
#include <stdlib.h>

/*
.text:0000A334                 addiu   $v0, $a0, -0xA   # switch 6 cases
.text:0000A338                 sltiu   $at, $v0, 6
.text:0000A33C                 beqz    $at, def_A354    # jumptable 0000A354
.text:0000A340                 nop
.text:0000A344                 sll     $at, $v0, 2
.text:0000A348                 lui     $v0, %hi(jpt_A354)
.text:0000A34C                 addu    $at, $v0
.text:0000A350                 lw      $at, %lo(jpt_A354)($at)
.text:0000A354                 jr      $at              # switch jump
.text:0000A358                 nop
 */


/*
.data:0000AC54 jpt_A354:       .word loc_A35C           # DATA XREF:
.data:0000AC54                                          # process_type+1C↑o
.data:0000AC54                                          # process_type+24↑r
.data:0000AC58                 .word loc_A37C
.data:0000AC5C                 .word loc_A394
.data:0000AC60                 .word loc_A3B0
.data:0000AC64                 .word loc_A3C8
.data:0000AC68                 .word loc_A3E0
 */
__attribute__((noinline)) int process_type(int type) {
  switch (type) {
  case 10:
    printf("linde %d\n", __LINE__);
    return 312;
  case 11:
    printf("lxxine %d\n", __LINE__);
    return 42345;
  case 12:
    printf("liadsfne %d\n", __LINE__);
    return 423423;
  case 13:
    printf("liasdfne %d\n", __LINE__);
    return 1232;
  case 14:
    printf("linxcve %d\n", __LINE__);
    return 65465;
  case 15:
    printf("liasdfne %d\n", __LINE__);
    return 767;
  default:
    return -1;
  }
}

int main(int argc, char *argv[]) {
  for (int i = 0; i < 20; ++i)
    printf("input %d return %d\n", i, process_type(i));
  return 0;
}
