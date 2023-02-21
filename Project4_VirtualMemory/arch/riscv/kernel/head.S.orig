/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define pid0_stack 0x50501000
#define pid0_stack_slave 0x50502000


.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bnez t0, slave_init_stack


  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la s0, __bss_start
  la s1, __BSS_END__
  li s2, 0
bss_loop:
  sw s2, (s0)
  addi s0, s0, 1
  blt s0, s1, bss_loop

  /* TODO: [p1-task2] setup C environment */
  la sp, pid0_stack
  la tp, pid0_pcb
  j slave_stack_finish

slave_init_stack:
  la sp, pid0_stack_slave
  la tp, pid0_pcb_slave

slave_stack_finish:


  call main


loop:
  wfi
  j loop

END(_start)
