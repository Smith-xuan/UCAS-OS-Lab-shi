#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>

#define TASKS_INFO_BLOCK_ID_ADDR 0x502001fa
#define TASKS_INFO_lOC 0x50300000
#define TASK_NAME_MAXNUM 16


// Task info array
task_info_t tasks[NUM_MAX_TASK];



static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    short tasksInfoBlockId = *(short *)TASKS_INFO_BLOCK_ID_ADDR;
    bios_sdread(TASKS_INFO_lOC,1,tasksInfoBlockId);

    task_info_t *tasksInfoPtr = (task_info_t *)TASKS_INFO_lOC;
    for (int i = 0; i < TASK_MAXNUM; i++){
        for (int j = 0; j < TASK_NAME_MAXNUM; j++){
            tasks[i].task_name[j] = tasksInfoPtr[i].task_name[j];
        }
        tasks[i].task_offset = tasksInfoPtr[i].task_offset;
        tasks[i].task_size = tasksInfoPtr[i].task_size;
        tasks[i].TaskEntryPoint = tasksInfoPtr[i].TaskEntryPoint;
    }
    
}

void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for (int i = 0; i < 32; i++){
        pt_regs->regs[i] = 0;
    }
    pt_regs->regs[1] = entry_point; //ra
//    pt_regs->regs[2] = (ptr_t)pt_regs;  //sp
    pt_regs->regs[2] = user_stack;  //sp
    pt_regs->regs[4] = pcb;
    pt_regs->sepc = entry_point;    //sepc
    pt_regs->sstatus = (1 << 1);    //sstatus
    

    /* TODO: [p2-task1] set sp to simulate just returning from s witch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pt_switchto->regs[0] = &ret_from_exception;//ra
    pt_switchto->regs[1] = user_stack;//sp  
    pcb -> kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
    pcb -> user_sp   = user_stack;
    pcb -> entry_point = entry_point;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    init_list_head(&ready_queue);


    for (int j = 0; j < 15; j++){
        if (j == 0){
            uint64_t ENTRY_POINT = load_task_img(j);
            pcb[j].pid = j+1;
            pcb[j].cpu_id = 0;
            pcb[j].type = 0;
            pcb[j].status = TASK_READY;
            pcb[j].name = tasks[j].task_name;
            init_list_head(&(pcb[j].wait_list));
            list_add_tail(&pcb[j].list,&ready_queue);
            uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
            uint64_t user_stack = allocUserPage(1) + PAGE_SIZE;
            init_pcb_stack(kernel_stack,user_stack,ENTRY_POINT,&(pcb[j]));
        }
        else{
            load_task_img(j);
            pcb[j].pid = 0;
            pcb[j].status = TASK_EXITED;
        }
        pcb[j].tid = 0;
        pcb[j].lock_num = 0;
        pcb[j].thread_num = 0;
        pcb[j].thread_idx = 0;
        pcb[j].type = 0;
    }
    
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running[0] = &pid0_pcb;
    next_running[0] = &pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    for (int i = 0; i < NUM_SYSCALLS; i++){
        syscall[i] = &handle_other;
    }
    syscall[SYSCALL_SLEEP] = &do_sleep;
    syscall[SYSCALL_YIELD] = &do_scheduler;
    syscall[SYSCALL_WRITE] = &screen_write;
    syscall[SYSCALL_CURSOR] = &screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = &screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = &get_time_base;
    syscall[SYSCALL_GET_TICK] = &get_ticks;
    syscall[SYSCALL_LOCK_INIT] = &do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = &do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = &do_mutex_lock_release;
    syscall[SYSCALL_THREAD_CREATE] = &thread_create;

    //prj 3
    syscall[SYSCALL_GET_CHAR] = &bios_getchar;
    syscall[SYSCALL_PUT_CHAR] = &bios_putchar;
    syscall[SYSCALL_PS] = &do_process_show;
    syscall[SYSCALL_CLEAR] = &screen_clear;
    syscall[SYSCALL_KILL] = &do_kill;
    syscall[SYSCALL_EXIT] = &do_exit;
    syscall[SYSCALL_WAITPID] = &do_waitpid;
    syscall[SYSCALL_GETPID] = &do_getpid;
    syscall[SYSCALL_EXEC] = &do_exec;

    syscall[SYSCALL_BARR_INIT] = &do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = &do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = &do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = &do_condition_init;
    syscall[SYSCALL_COND_WAIT] = &do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = &do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = &do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = &do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN] = &do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = &do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = &do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = &do_mbox_recv;
    syscall[SYSCALL_BACKSPACE] = &screen_backspace;
    syscall[SYSCALL_TASKSET] = &do_taskset;
    syscall[SYSCALL_TASKSET_RUN] = &do_tasksetrun;
}

int main(void)
{
    volatile char *flagflag = (void *) 0x59000000;
    *flagflag = 0;
    
    uint64_t cpu_id = get_current_cpu_id();
    if(cpu_id == 0){
    // master core

        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();
        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        init_barriers();
        init_conditions();
        init_mbox();

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        printk("the master core ok.\n");
        smp_init();
        lock_kernel();
        wakeup_other_hart();

        let_ipi_go();// clear the sip to keep the master core from ipi interrupt
        setup_exception();
    }else{
        //lock the kernel
        lock_kernel();
        //ready for switch_to
        current_running[1] = &pid0_pcb_slave;
        next_running[1] = &pid0_pcb_slave;
        setup_exception();

        printk("the slave core ok.\n");
    }




    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    reset_timer();
    unlock_kernel();
    enable_interrupt();
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        //do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        //asm volatile("wfi");
    }
 
    return 0;
}
