#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

spin_lock_t kernel_lock;

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    kernel_lock.status = UNLOCKED;
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    send_ipi(NULL);
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    while (atomic_swap(LOCKED, (ptr_t)(&(kernel_lock.status)))){
        ;
    }
    
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    kernel_lock.status = UNLOCKED;
}
