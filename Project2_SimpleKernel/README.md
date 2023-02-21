Project2 part 2(task3 - task5)

一、设计流程：
1）对系统调用（例外）的处理
当一个用户程序发生以系统调用为例的例外时，其汇编会执行一条ecall指令，这条指令是从用户态陷入内核态的指令，首先会引起机器的反应。cpu会自动屏蔽其他中断信号，保存发生例外之前的PIE位，例外处理结束后自动返回该PIE位；此外机器还会将发生例外的PC储存在sepc寄存器中，将系统调用号（例外原因）存储在scause寄存器中；最后自动跳转到stvec寄存器存储的地址（操作系统做过初始化）处，然后交给操作系统处理。
而操作系统在进入用户程序之前，已经做了一些准备工作：初始化例外处理跳转表和中断处理跳转表、初始化系统调用跳转表、把异常处理入口函数（exception_handler_entry）的地址存入stvec寄存器中。
因而，当机器自动跳转到exception_handler_entry处，操作系统就开始了对例外的处理：首先是要保存上下文，即用SAVE CONTEXT汇编函数将所有的通用寄存器和状态寄存器的值存在事先初始化好的栈上，并且由于系统调用时相关参数都保存在寄存器中，所以只要设计时让通用寄存器存储的栈底regs传到异常处理跳转函数interrupt_helper，再通过查找例外跳转表（若有中断就是中断跳转表）跳转到对应的例外处理函数handle_syscall，然后通过regs中存储的相关参数找到对应的系统调用执行后返回。
这些函数执行完后会一级一级退栈，最后退回exception_handler_entry，退回后加载函数ret_from_exception的地址，跳到该函数入口，进行RESTORE CONTEXT把存入的通用寄存器和状态寄存器的值取出后，用sret返回内核态，这样一次系统调用就完成了。

值得注意的是，sepc的值在内核处理时要+4，因为用sret返回内核态时，实际上是机器检测sepc的值并返回存储的地址，这样我们就需要用+4跳过系统调用指令，否则将不断进行重复调用。此外，栈的位置设置也很关键，要考虑到每次开始存的时候一定都存对位置，而且传参时一定要把栈底传过去，还要考虑在进行syscall_yield系统调用的时候，需要做调度，而do_schedluer的栈应该跟task2版本的有所区别。


2）对时钟中断（中断）的处理
时钟中断实际上整体的架构与系统调用是很像的，每次时钟中断实际上就是调用bios_set_timer，然后进行调度。这样在用户程序中往下执行时隔一段设置好的时间后就会自动发出中断请求，然后跟系统调用的那套例外处理几乎一样，只不过在异常处理跳转函数interrupt_helper会从时钟跳转表选择处理函数，而处理函数handle_irq_timer的实现也很简单，直接复用bios_set_timer和调度就可以了。
这样，我们就摆脱了syscall_yield这种非抢占式调度，转而在每一次时钟中断引发调度时进行抢占式调度（虽然由于FIFO的简单调度算法，抢占的意味还不明显）。


3）对创建线程的处理
创建线程需要在内核中单独写一个处理函数thread_create，在初始化系统调用的跳转表的时候将这个函数添加上去。这样我们就可以复用系统调用的处理过程，将创建线程当做是普通的例外处理。我们可以准备一定数量的pcb用作tcb（创建出的线程对应的控制用数据结构），每次有创建新线程的请求，我们就从事先准备好的tcb中取出一个进行初始化，包括pid，tid等。线程的pid应与目前在运行的进程（即进行系统调用的进程）保持一致，tid从某个数开始累加，创建的线程越多，tid就越大。主进程的数据结构中也添加了用于指示从线程数量的元素。还可以复用函数init_pcb_stack来初始化tcb的栈，基本方式与pcb相同，不同的地方在于入口地址要变成用户程序中线程要做的函数入口。这也是线程共享资源的原因，只要最终进入同一个程序，即使在不同的函数中也可以共享全局变量和静态变量。每创建一个线程，就把其放入准备队列中，之后当做进程处理即可。


二、关键代码添加：
1）exception_handler_entry函数（位于trap.S）

    ENTRY(exception_handler_entry)
    SAVE_CONTEXT

    mv   a0, sp
    csrr a1, CSR_STVAL
    csrr a2, CSR_SCAUSE
    call interrupt_helper

    la ra, ret_from_exception
    jr ra
    ENDPROC(exception_handler_entry)

1. 保存上下文   2. 调用异常处理函数interrupt_helper（传参）  3.跳入ret_from_exception函数进行收尾处理


2）interrupt_helper（位于irq.c）

    void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
    {
        handler_t *table = (scause >> 63)? irq_table: exc_table;
        scause = scause << 1;
        scause = scause >> 1;              // clear the 64th bit
        table[scause](regs, stval, scause);// invoke the function
    }

1.用scause中的值判断是例外还是中断选择跳转表   2.得到跳转表中的操作序号   3.执行跳转表中对应的函数


3）handle_syscall函数（位于syscall.c（内核态））

    void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
    {
        regs->sepc = regs->sepc + 4;
        regs->regs[10] = syscall[regs->regs[17]](regs->regs[10], regs->regs[11], regs->regs[12]);
    }

1.sepc + 4，从而让跳转回用户态时略过系统调用那条指令   2.从通用寄存器存储所在的栈处取出对应参数，从而进行正确的系统调用函数跳转和执行


4）handle_irq_timer和reset_timer函数（位于irq.c）

    void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
    {
        reset_timer();
    }

    void reset_timer()
    {
        bios_set_timer(get_ticks() + get_time_base() / 1000);
        do_scheduler();
    }

1. 时钟中断时的处理：即重新设置下次中断时间，进行调度，这里的时间片为1/1000s


5）thread_create函数（位于sched.c）

    int thread_create(pid_t *thread, void(*thread_func)(void *), char *argv[])
    {
        thread_idx++;
        next_running->thread_num++;
        pcb_t *new_tcb = &tcb[thread_idx];
        new_tcb->thread_idx = thread_idx;
        new_tcb->pid = next_running->pid;
        new_tcb->tid = next_running->thread_num;
        new_tcb->status = TASK_READY;

        list_add_tail(&new_tcb->list,&ready_queue);

        uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
        uint64_t user_stack = allocUserPage(1) + PAGE_SIZE;
        uint64_t entry = (uint64_t)(*thread_func);
        init_pcb_stack(kernel_stack,user_stack,entry,new_tcb);

        return new_tcb->pid;
    }

1. 移动全局线程指针，给当前进程的从线程数目加一   2. 用全局线程指针得到新的tcb块，并对其基本属性初始化（pid，tid，status，全局线程指针位置）  3.加入准备队列   4.开栈空间，并进行栈的初始化（入口地址设置为特定的函数地址入口）


