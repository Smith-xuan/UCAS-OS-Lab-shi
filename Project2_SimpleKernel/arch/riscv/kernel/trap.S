#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la s0, exception_handler_entry
  csrw CSR_STVEC, s0
  
  /* TODO: [p2-task4] enable interrupts globally */
  li s0, SR_SIE
  csrw CSR_SSTATUS, s0  

  li s0, SIE_STIE | SIE_SEIE | SIE_SSIE
  csrw CSR_SIE, s0


  ret

ENDPROC(setup_exception)
