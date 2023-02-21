Project3

一、设计流程：
1）为实现shell而添加的系统调用
本次实验首先是要添加一个shell，而要实现用户命令的读取、解析、显示，需要实现新的几种系统调用。这里主要实现了exec、exit、kill、waitpid这四种系统调用。

exec系统调用要完成的内容是给指定的进程分配一个可用pcb，再给这个进程的pcb做一些必要的初始化（包括内核栈的初始化）。需要注意的是，这里的初始化与原来相比多出的部分在于要在此处将exec系统调用传入的参数按照一定的顺序组织并放在用户栈中（需要保证128bit对齐），以供新创建出的进程在执行main函数时能直接调用到传入的参数。

exit和kill这两种系统调用在目前的环境下要做的事情很接近，都是要把某个进程的内容给“清空”。主要要做的都是释放要exit或者kill的进程当前所持有的所有锁、让当前所有被阻塞在该进程的waitlist上的所有其他进程都释放掉并放入ready_queue中、最后设置一下pcb的状态，使得pcb再次成为可用状态。exit和kill不同的地方主要在于kill需要通过传入的pid值去索引一下要删除的进程，而exit是要退出当前在跑的进程。

waitpid要做的事也很简单，就是通过传入的pid索引一下要等待的进程的pcb，然后把当前进程挂在要等待的进程的wait_list上阻塞，等待这个进程被kill或者自己exit了重新进入调度队列。

2）几种进程间通信的系统调用实现
本次实验添加的进程间通信方式有屏障（barrier）、条件变量（condition）和邮箱（mailbox）这几种。

barrier的实现主要是设置了一个goal_num和一个now_num，最初的时候把nownum初始化为0，在某个进程首先初始化时设定了goalnum之后，每次有进程调用barrier,内核中nownum都会加一，但是在nownum没达到goalnum之前总把进程挂在阻塞队列中，直到最后一个要调用barrier的进程进入内核，goalnum达到，此时释放所有原来在阻塞队列中的进程即可。

condition的实现则借助了互斥锁，资源不满足时用wait原语释放互斥锁，挂在condition自己的阻塞队列中，等待其他进程唤醒后再得到锁。其他进程如果让条件得以满足，需要用signal或者broadcast原语把阻塞队列中的进程释放出来。

mailbox的实现稍微复杂一些，借用了互斥锁和条件变量，完成了对一个8byte的邮箱满则不填、空则不取的实现，在满足可填可取的情况下设置指针实现邮件的先进先出。

3）双核的实现
本次实验实现了双核，双核从bootblock时行为发生分歧。在bootblock时主核依然是将内核代码读到内存后跳转到head.s中清空bss段设置栈指针，而对于从核来说在bootblock不需要再读内核代码到内存，只需要改变一下控制状态寄存器的值使得从核只能接受核间中断，并设置一下stvec寄存器中的值（核间中断来的时候要跳转到哪，这里设置为head.s）让后让其进入死循环，等待主核唤醒。

主核从head.s继续跳转到main函数中做一众事项的初始化，在这个初始化做完之后，我们拿取大内核锁（在smp.c中使用原子指令核自旋锁实现），发送核间中断，并消除核间中断对主核本身的影响。之后主核就去设置时钟中断并做调度到初始化好的shell进程中（在进入shell前会释放大内核锁）。至于从核则在接收到核间中断的时刻跳转到head.s设置了栈指针后（不需要再清空bss段），也跳转进入main中，不过不需要初始化，直接拿取内核锁，开中断后也设置时钟中断做调度即可。

但是这里设置双核之后存在一个关键的问题，就是存在一个核把可调度唯一进程给拿走了，这时另一个核就会在do_scheduler中找不到可以调度的进程。这时我们就要使用pid0这一空泡程序，它会使得该核在无可调度时继续跑自己原本的进程。

4）绑核功能的实现
实现绑核主要是在pcb中加了一个type变量，当type=0的时候说明在两个核上都可以调度，type=1则只能在主核，=2则只能在从核。系统调用taskset时，我们需要先把进程创建出来，然后给这个进程pcb的type附上我们预想的值，做调度的时候针对是否满足type与cpuid的对应决定是不是该调度。系统调用taskset -p时，我们要根据传入的pid索引进程的pcb，并给type赋值。当然主要是再调度队列中添加了如果type核cpuid不对应就去找下一个可调度进程的代码。

二、关键代码添加：

1）do_exec(在sched.c)

    pid_t do_exec(char *name, int argc, char *argv[])
    {
        process_id++;
        int status = 0;
        uint64_t cpu_id = get_current_cpu_id();
    /*    for (int i = 0; i < NUM_MAX_TASK; i++){
            if(!strcmp(name,pcb[i].name)){
                pcb[i].pid = process_id;
                list_add_tail(&pcb[i].list,&ready_queue);
                regs_context_t *pt_regs = (regs_context_t *)(pcb[i].kernel_sp + sizeof(switchto_context_t));
                pt_regs->regs[10] = argc;
                
            }

        }*/
        int i = 0;
        for (i = 0; i < NUM_MAX_TASK; i++){
            if(!strcmp(name,tasks[i].task_name)){
            //    uint64_t ENTRY_POINT = load_task_img(i);
                uint64_t ENTRY_POINT = tasks[i].TaskEntryPoint;
                for (int j = 0; j < NUM_MAX_TASK; j++){
                    if(pcb[j].pid == 0){
                        pcb[j].pid = process_id;
                        init_list_head(&(pcb[j].wait_list));
                        pcb[j].status = TASK_READY;
                        pcb[j].type = next_running[cpu_id]->type;
                        pcb[j].name = name;
                        list_add_tail(&(pcb[j].list),&ready_queue);
                        uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
                        uint64_t user_stack = allocUserPage(1) + PAGE_SIZE;
                        uint64_t argv_base = user_stack - argc * sizeof(ptr_t);

                        //copy the argv in the user stack and store the ptr
                        ptr_t *p1,*p2;
                        p1 = (ptr_t *)argv_base;
                        p2 = (ptr_t *)argv_base;

                        for (int m = 0; m < argc; m++){
                            p2 = (ptr_t *)((uint64_t)p2 - 16*sizeof(char));
                            p2 = (ptr_t *)strcpy(p2, argv[m]);
                            *p1 = (uint64_t)p2;
                            p1 = (ptr_t *)((uint64_t)p1 + sizeof(uint64_t));
                        }

                        //128 bit alien 
                        int more = (user_stack - (uint64_t)p2) % 128;
                        if(more != 0){
                            int to_add = 128-more;
                            p2 = (ptr_t *)((uint64_t)p2 - to_add);
                        }
                        user_stack = (uint64_t)p2;
                        

                        //init the kernel stack
                        regs_context_t *pt_regs =
                            (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

                        for (int n = 0; n < 32; n++){
                            pt_regs->regs[n] = 0;
                        }
                        pt_regs->regs[1] = ENTRY_POINT; //ra
                        pt_regs->regs[2] = user_stack;  //sp
                        pt_regs->regs[4] = &(pcb[j]);
                        pt_regs->regs[10] = argc;
                        pt_regs->regs[11] = argv_base;
                        pt_regs->sepc = ENTRY_POINT;    //sepc
                        pt_regs->sstatus = (1 << 1);    //sstatus

                        switchto_context_t *pt_switchto =
                            (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

                        pt_switchto->regs[0] = &ret_from_exception;//ra
                        pt_switchto->regs[1] = user_stack;//sp  
                        pcb[j].kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
                        pcb[j].user_sp   = user_stack;
                        pcb[j].entry_point = ENTRY_POINT;

                        //exec success flag
                        status = pcb[j].pid;
                        break;
                    }
                }
                
            }
        }
        return status;
    }

2）进程间通信代码，以condition为例(lock.c中)

    void init_conditions(void)
    {
        for (int i = 0; i < CONDITION_NUM; i++){
            init_list_head(&(cond[i].block_queue));
            cond[i].status = 0;
        }
        
    }

    int do_condition_init(int key)
    {
        int cond_idx = key % 10;
        cond[cond_idx].status = 1;
        cond[cond_idx].block_count = 0;
        return cond_idx;
    }

    void do_condition_wait(int cond_idx, int mutex_idx)
    {
        uint64_t cpu_id = get_current_cpu_id();
        do_mutex_lock_release(mutex_idx);

        do_block(&(next_running[cpu_id]->list), &(cond[cond_idx].block_queue));

        do_mutex_lock_acquire(mutex_idx);
    }

    void do_condition_signal(int cond_idx)
    {
        do_unblock(&(cond[cond_idx].block_queue));
    }

    void do_condition_broadcast(int cond_idx)
    {
        while(!list_empty(&(cond[cond_idx].block_queue))){
            do_unblock(&(cond[cond_idx].block_queue));
        }
    }

    void do_condition_destroy(int cond_idx)
    {
        while(!list_empty(&(cond[cond_idx].block_queue))){
            do_unblock(&(cond[cond_idx].block_queue));
        }
        cond[cond_idx].status = 0;
    }

3）双核的部分代码(bootblock.s main.c smp.c中)
    (bootblock.s)
    secondary:
        /* TODO [P3-task3]: 
        * 1. Mask all interrupts
        * 2. let stvec pointer to kernel_main
        * 3. enable software interrupt for ipi
        */
        fence

        csrw CSR_SIE, zero // disable all kinds of interrupt

        la t0, kernel // ?
        csrw CSR_STVEC, t0 // when the ipi interruput comes, jump to kernel

        li t0, SIE_SSIE
        csrs CSR_SIE, t0 //enable ipi interrupt

        li t0, SR_SIE
        csrw CSR_SSTATUS, t0 //same to above


    (main.c)
    smp_init();
    lock_kernel();
    wakeup_other_hart();

    let_ipi_go();// clear the sip to keep the master core from ipi interrupt
    setup_exception();

    (smp.c)
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

4）绑核部分的代码(sched.c中)

do_scheduler中:

        list_node_t *p1;
        pcb_t *p2;
        if(!list_empty(&ready_queue)){
            p1 = ready_queue.next;
            p2 = (pcb_t *)((uint64_t)(p1) - 4*sizeof(reg_t));
            while((p2->type != 0) && !((p2->type == 1) && (cpu_id == 0)) && !((p2->type == 2) && (cpu_id == 1)) && (p1 != &ready_queue)){ //taskset core
                p1 = p1->next;
                p2 = (pcb_t *)((uint64_t)(p1) - 4*sizeof(reg_t));
            }
            if(p2 == &ready_queue){
                if(cpu_id == 0){
                    next_running[cpu_id] = &pid0_pcb;
                }
                else if(cpu_id == 1){
                    next_running[cpu_id] = &pid0_pcb_slave;
                }
            }else{
                next_running[cpu_id] = p2;
                list_del(&(next_running[cpu_id]->list));
            }
            
        }
        else if(cpu_id == 0){
            next_running[cpu_id] = &pid0_pcb;
        }else if(cpu_id == 1){
            next_running[cpu_id] = &pid0_pcb_slave;
        }

do_taskset中:

        pid_t do_taskset(char *name, int argc, char **argv, int type)
        {
            int taskpid = do_exec(name, argc, argv);
            for (int i = 0; i < NUM_MAX_TASK; i++){
                if(pcb[i].pid == taskpid){
                    pcb[i].type = type;
                    break;
                }
            }
            return taskpid;
        }
