#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
  addi sp, sp, -8
  sd ra, 0(sp)

  la s0, __bss_start
  la s1, __BSS_END__
  li s2, 0
bss_clear_loop:
  sw s2, (s0)
  addi s0, s0, 1
  blt s0, s1, bss_clear_loop

bss_clear_end:
    /* TODO: [p1-task3] enter main function */
  call main

    /* TODO: [P3] call sys_exit */
  call sys_exit
    /* TODO: [p1-task3] finish task and return to the kernel */
    /* NOTE: You need to replace this with new mechanism in p3-task2! */
ret_kernel:
  ld ra, 0(sp)
  addi sp, sp, 8
  ret

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
