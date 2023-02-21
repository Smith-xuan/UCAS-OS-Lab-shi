#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0
  
  /* TODO: [p2-task4] enable interrupts globally */
  li t0, SR_SIE | SR_SUM
  csrw CSR_SSTATUS, t0  

  li t0, SIE_STIE | SIE_SSIE //| SIE_SEIE
  csrw CSR_SIE, t0


  ret

ENDPROC(setup_exception)


ENTRY(let_ipi_go)

  li t0, 0
  csrw CSR_SIP, t0

  ret

ENDPROC(let_ipi_go)