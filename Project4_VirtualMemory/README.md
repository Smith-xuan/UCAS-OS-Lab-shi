Project4

一、设计流程：
1）开启虚存机制
本次实验的第一步是要开启虚存机制，在最早bootblock加载了内核之后，跳转到start.s简单屏蔽一下中断并且跳进boot.c，在boot.c中，主核会按照4MB模式先把虚存的内核高地址部分映射到物理地址（线性映射），同时把boot部分的物理地址当成虚拟地址直接映射到物理地址的相同位置，以便在使用set_satp开启虚存之后仍能继续在boot.c中继续往下执行。当然，主核建立了内核的4MB页表并开启虚存模式，从核就只需要复用内存中建立好的页表直接开启虚存模式即可。之后就是正常的一些初始化并跳入main.c，而在main.c中，主核首先要做的就是要把刚刚boot部分的直接映射给取消掉，即把相应的页表项无效化，以防止影响后面的访存，但值得注意的是此时可能从核还没有从boot.c中出来，所以我设置了一个标志位，使得主核进入main.c中后先拿到锁等待从核也进入main.c中后把该标志位置高，然后再取消直接映射，之后跟正常的运行就差别不大了。

2）初始化进程时的改变
初始化进程，如init_pcb中初始化shell,do_exec中初始化其他进程，在这些过程中，由于涉及到从sd卡中把进程代码load出来以及设置用户栈和内核栈，这些都是要从原来的物理地址转换成虚拟地址的，因而会发生改动。首先代码段都要load到0x10000开头的位置，内核栈直接放在内核高地址处，用户栈放在0xf00010000处。为了实现这样的布局，在mm.c中实现了alloc_page_helper等在获取确定的虚拟地址处一页大小的内存后并将其映射到物理地址的函数（建立页表项）。有了这些函数，我们在物理页框上看起来虽然好像跟之前差别不大，但对于进程本身来说，可见的虚拟地址空间是完全受进程本身控制的。

3）初始化进程的栈
在do_exec中，我们会有一个在老进程的虚拟地址空间下建立新进程页表的过程，这时会涉及到一些跨页表的动作。此时高地址的内核空间就显示出了它的作用，因为高地址空间实际上是存储了所有进程的页表的，它通过线性映射直接映射到物理地址空间。因此，当我们去初始化新进程的用户栈时，需要在高地址的内核空间写入，如果在低地址的用户空间写入，则由于在旧进程的页表中没有映射这新进程的用户栈地址而出错。但是真正写入的内容应该是低地址用户空间的内容。


4）缺页和换页机制的实现
在本次的实验环境中默认tlb的充填是硬件自动去做了，因此我们要处理的意外就是缺页，也就是页无效。当出现页无效的时候我们需要分几种情况分析，首先是有没有可能是权限异常，对于对应页表项A位和D位的判定；其次是有没有可能是页已经被换到磁盘中去了，对于这个换页的机制，我们可以在pcb中设置一个线程占有页的队列，如果超过某个数字，我们就把超过的部分给存到磁盘中去，并在队列中标识上该页已经在磁盘中了，这样的话，每次我们触发缺页时可以比对pcb中的该队列中的虚拟地址和是否在磁盘中，如果触发缺页的虚拟地址与pcb中该队列的占有页的虚拟地址在同一页中且被标识上在磁盘，则需要把该页从磁盘中取出。如果上述情况都不满足，我们就认为是最普通的缺页，即访问的虚拟地址没有被映射过，那么我们就根据该虚拟地址给他分配新的一页并建立映射，再加入到当前线程的页持有队列中即可。

5）线程创建
线程的创建在虚存背景下显得十分简单，因为新创建的线程跟主线程共用一个页表，不需要load代码段，入口地址直接设置成主线程load的代码段中的某个函数即可，且只需要传递一个参数，也不需要设置用户栈，因而难度不大。

6）共享内存
此功能在O0下能实现，O2下双核未能实现。具体思路就是把共享的内存当成像lock那样的内核共享资源，并设置一个专门的类似于pcb的结构体去控制它。这个结构体中记录了共享内存页的物理地址，进行共享的线程数目，以及这些进行共享的线程对应该共享页的虚地址。这样的话，我们就可以根据传入的key去寻找对应的共享页，并且每有一个线程来获取该共享页，进行了映射后，我们就把相关的线程信息记录下来，释放共享页的时候进行一个相反的过程即可。

二、关键代码添加：

1）alloc_page_helper(在mm.c)

    uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
    {
        // TODO [P4-task1] alloc_page_helper:
        va = va & VA_MASK;
        uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
        uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
        uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS +PPN_BITS)) & ~(~0 << PPN_BITS);

        PTE *pte2 = (PTE *)pgdir + vpn2;
        PTE *pte1 = NULL;
        PTE *pte0 = NULL;
        //invalid
        if(((*pte2) & 0x1) == 0){
            ptr_t pt1_va = allocPage();
            uintptr_t pt1_pa = kva2pa(pt1_va);
            //upload the pte
            *pte2 = (pt1_pa >> 12) << 10;
            set_attribute(pte2, _PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(*pte2)));
            pte1 = (PTE *)pt1_va + vpn1;
        }else{
            ptr_t pt1_pa = (*pte2 >> 10) << 12;
            uintptr_t pt1_va = pa2kva(pt1_pa);

            pte1 = (PTE *)pt1_va + vpn1;
        }


        if(((*pte1) & 0x1) == 0){
            ptr_t pt0_va = allocPage();
            uintptr_t pt0_pa = kva2pa(pt0_va);
            //upload the pte
            *pte1 = (pt0_pa >> 12) << 10;
            set_attribute(pte1, _PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(*pte1)));
            pte0 = (PTE *)pt0_va + vpn0;
        }else{
            ptr_t pt0_pa = (*pte1 >> 10) << 12;
            uintptr_t pt0_va = pa2kva(pt0_pa);

            pte0 = (PTE *)pt0_va + vpn0;
        }


        if(((*pte0) & 0x1) != 0){
            return 0; // the va has been mapped
        }
        ptr_t final_va = allocPage();
        uintptr_t final_pa = kva2pa(final_va);
        set_pfn(pte0, final_pa >> NORMAL_PAGE_SHIFT);

        set_attribute(pte0, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_USER| _PAGE_ACCESSED | _PAGE_DIRTY);

        return final_va;
    }


2）load_task_img(loader.c中)

    uint64_t load_task_img(int taskid, uintptr_t pgdir)
    {
        /**
        * TODO:
        * 1. [p1-task3] load task from image via task id, and return its entrypoint
        * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
        */
        unsigned int block_num = tasks[taskid].mem_size;
        unsigned int fileblock_num = tasks[taskid].task_size;
        unsigned int block_entry = tasks[taskid].task_offset;
    //    uint64_t ENTRY_POINT = TASK_MEM_BASE+taskid*TASK_SIZE;
    //    uint64_t ENTRY_POINT = tasks[taskid].TaskEntryPoint;
        int pagenum = (block_num / 8) + 1;
        int filepagenum = (fileblock_num / 8) + 1;
        for (int i = 0; i < pagenum; i++){
            if(i < filepagenum){ // load the text
                uintptr_t kva_text = alloc_page_helper(USER_TEXT_BASE+i*PAGE_SIZE, pgdir);
                int block_id = block_entry + i*8;
                bios_sdread(kva2pa(kva_text), 8, block_id);
            }else{ // bss
                uintptr_t kva_bss = alloc_page_helper(USER_TEXT_BASE+i*PAGE_SIZE, pgdir);
                for (int j = 0; j < 512; j++){
                    *((uint64_t*)kva_bss + j) = 0;
                }
            }
        }
        uint64_t ENTRY_POINT = USER_TEXT_BASE;
    //   bios_sdread(ENTRY_POINT,block_num,block_entry);

        return ENTRY_POINT;
    }

3）do_exec(sched.c中)

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
            //    uint64_t ENTRY_POINT = tasks[i].TaskEntryPoint;
                for (int j = 0; j < NUM_MAX_TASK; j++){
                    if(pcb[j].pid == 0){
                        pcb[j].pid = process_id;
                        init_list_head(&(pcb[j].wait_list));
                        init_list_head(&(pcb[j].plist));
                        pcb[j].status = TASK_READY;
                        pcb[j].type = next_running[cpu_id]->type;
                        pcb[j].name = name;
                        list_add_tail(&(pcb[j].list),&ready_queue);

                        //init page table && load the image
                        pcb[j].pgdir = allocPage();
                        memcpy((char*)pcb[j].pgdir, (char*)pa2kva(PGDIR_PA), PAGE_SIZE);
                        uintptr_t kernel_stack = allocPage() + PAGE_SIZE;
                        uintptr_t user_stack = alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE, pcb[j].pgdir) + PAGE_SIZE;
                        uintptr_t user_stack_pre = user_stack;
                        uint64_t ENTRY_POINT = load_task_img(i,pcb[j].pgdir);
                        uint64_t argv_base = user_stack - argc * sizeof(ptr_t);

                        //copy the argv in the user stack and store the ptr
                        ptr_t *p1,*p2;
                        p1 = (ptr_t *)argv_base;
                        p2 = (ptr_t *)argv_base;

                        for (int m = 0; m < argc; m++){
                            p2 = (ptr_t *)((uint64_t)p2 - 16*sizeof(char));
                            p2 = (ptr_t *)strcpy(p2, argv[m]);
                            *p1 = (uint64_t)(USER_STACK_ADDR - (user_stack - (uint64_t)p2)); // the argv point should be user addr
                            p1 = (ptr_t *)((uint64_t)p1 + sizeof(uint64_t));
                        }

                        //128 bit alien 
                        int more = (user_stack - (uint64_t)p2) % 16;
                        if(more != 0){
                            int to_add = 16-more;
                            p2 = (ptr_t *)((uint64_t)p2 - to_add);
                        }
                        user_stack = (uint64_t)p2;
                        
                        //record the sp offset
                        uint64_t arg_offset = user_stack_pre - user_stack;

                        //init the kernel stack
                        regs_context_t *pt_regs =
                            (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

                        for (int n = 0; n < 32; n++){
                            pt_regs->regs[n] = 0;
                        }
                        pt_regs->regs[1] = ENTRY_POINT; //ra
                        pt_regs->regs[2] = USER_STACK_ADDR - arg_offset;  //sp
                        pt_regs->regs[4] = &(pcb[j]);
                        pt_regs->regs[10] = argc;
                        pt_regs->regs[11] = USER_STACK_ADDR - argc * sizeof(ptr_t);
                        pt_regs->sepc = ENTRY_POINT;    //sepc
                        pt_regs->sstatus = (1 << 1);    //sstatus

                        switchto_context_t *pt_switchto =
                            (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

                        pt_switchto->regs[0] = &ret_from_exception;//ra
                        pt_switchto->regs[1] = USER_STACK_ADDR - arg_offset;//sp  
                        pcb[j].kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
                        pcb[j].user_sp   = USER_STACK_ADDR - arg_offset;
                        pcb[j].entry_point = ENTRY_POINT;

                        //update pgdir && alloc two page_t for usr and kernel stack
                        pcb[j].pgdir = kva2pa(pcb[j].pgdir);

                        // page_t *ker_stack = (page_t*)kmalloc(sizeof(page_t));
                        // ker_stack->pa = kva2pa(kernel_stack - PAGE_SIZE);
                        // ker_stack->va = kernel_stack - PAGE_SIZE;
                        // init_list_head(&(ker_stack->list));
                        // list_add_tail(&(ker_stack->list), &(pcb[j].plist));

                        page_t *usr_stack = (page_t*)kmalloc(sizeof(page_t));
                        usr_stack->pa = kva2pa(user_stack_pre - PAGE_SIZE);
                        usr_stack->va = USER_STACK_ADDR - PAGE_SIZE;
                        usr_stack->in_disk = 1;
                        init_list_head(&(usr_stack->list));
                        list_add_tail(&(usr_stack->list), &(pcb[j].plist));

                        pcb[j].pnum = 1;
                        //exec success flag
                        status = pcb[j].pid;
                        break;
                    }
                }
                
            }
        }
        return status;
    }

4）pthread_create(sched.c中)

    void pthread_create(pthread_t *thread, void (*start_routine)(void*), void *arg)
    {
        uint64_t cpu_id = get_current_cpu_id();
        thread_idx++;
    //record the thread id and some info in current_running and tcb    
        pcb_t *tcb_created = &tcb[thread_idx];

        next_running[cpu_id]->thread_idx[next_running[cpu_id]->thread_num] = thread_idx;
        next_running[cpu_id]->thread_num++;
        
        *thread = next_running[cpu_id]->thread_num;

        tcb_created->cpu_id = cpu_id;
        tcb_created->type = next_running[cpu_id]->type;
        tcb_created->pid = next_running[cpu_id]->pid;
        tcb_created->tid = *thread;
        tcb_created->status = TASK_READY;
        tcb_created->pgdir = pa2kva(next_running[cpu_id]->pgdir);

    //init the list
        init_list_head(&(tcb_created->list));
        init_list_head(&(tcb_created->plist));
        init_list_head(&(tcb_created->wait_list));
        list_add_tail(&(tcb_created->list), &ready_queue);

    //alloc and init the stack
        uintptr_t kernel_stack = allocPage() + PAGE_SIZE;
        uintptr_t user_stack = alloc_page_helper(USER_STACK_ADDR + (thread_idx-1)*PAGE_SIZE, tcb_created->pgdir) + PAGE_SIZE;
        uintptr_t ENTRY_POINT = start_routine;

        regs_context_t *pt_regs =
            (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

        for (int n = 0; n < 32; n++){
            pt_regs->regs[n] = 0;
        }
        pt_regs->regs[1] = ENTRY_POINT; //ra
        pt_regs->regs[2] = USER_STACK_ADDR + thread_idx*PAGE_SIZE;  //sp
        pt_regs->regs[4] = tcb_created;
        pt_regs->regs[10] = arg;
        pt_regs->sepc = ENTRY_POINT;    //sepc
        pt_regs->sstatus = (1 << 1);    //sstatus

        switchto_context_t *pt_switchto =
            (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

        pt_switchto->regs[0] = &ret_from_exception;//ra
        pt_switchto->regs[1] = USER_STACK_ADDR + thread_idx*PAGE_SIZE;//sp  
        tcb_created->kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
        tcb_created->user_sp  = USER_STACK_ADDR + thread_idx*PAGE_SIZE;
        tcb_created->entry_point = ENTRY_POINT;

        //share the pgdir
        tcb_created->pgdir = next_running[cpu_id]->pgdir;

        page_t *usr_stack = (page_t*)kmalloc(sizeof(page_t));
        usr_stack->pa = kva2pa(user_stack - PAGE_SIZE);
        usr_stack->va = USER_STACK_ADDR + (thread_idx-1)*PAGE_SIZE;
        usr_stack->in_disk = 1;
        init_list_head(&(usr_stack->list));
        list_add_tail(&(usr_stack->list), &(tcb_created->plist));
        tcb_created->pnum = 1;

    }


5) 共享内存代码(mm.c中)

    uintptr_t shm_page_get(int key)
    {
        // TODO [P4-task4] shm_page_get:
        uint64_t cpu_id = get_current_cpu_id();
        int shm_idx = key % MOST_SHARE_PAGE;
        ptr_t user_addr = SHARE_U_MEM_BASE + next_running[cpu_id]->shm_pgnum * PAGE_SIZE;
        ptr_t phy_addr = SHARE_P_MEM_BASE + shm_idx * PAGE_SIZE;
        ptr_t pgdir = pa2kva(next_running[cpu_id]->pgdir);

        next_running[cpu_id]->shm_pgnum++;
        shm[shm_idx].map_num++;
        shm[shm_idx].vaddr[shm[shm_idx].map_num-1] = user_addr;

        //map the useraddr to phyaddr
        uintptr_t user_addr_39 = user_addr & VA_MASK;
        uint64_t vpn0 = (user_addr_39 >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
        uint64_t vpn1 = (user_addr_39 >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
        uint64_t vpn2 = (user_addr_39 >> (NORMAL_PAGE_SHIFT + PPN_BITS +PPN_BITS)) & ~(~0 << PPN_BITS);

        PTE *pte2 = (PTE *)pgdir + vpn2;
        PTE *pte1 = NULL;
        PTE *pte0 = NULL;
        //invalid
        if(((*pte2) & 0x1) == 0){
            ptr_t pt1_va = allocPage();
            uintptr_t pt1_pa = kva2pa(pt1_va);
            //upload the pte
            *pte2 = (pt1_pa >> 12) << 10;
            set_attribute(pte2, _PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(*pte2)));
            pte1 = (PTE *)pt1_va + vpn1;
        }else{
            ptr_t pt1_pa = (*pte2 >> 10) << 12;
            uintptr_t pt1_va = pa2kva(pt1_pa);

            pte1 = (PTE *)pt1_va + vpn1;
        }


        if(((*pte1) & 0x1) == 0){
            ptr_t pt0_va = allocPage();
            uintptr_t pt0_pa = kva2pa(pt0_va);
            //upload the pte
            *pte1 = (pt0_pa >> 12) << 10;
            set_attribute(pte1, _PAGE_PRESENT);
            clear_pgdir(pa2kva(get_pa(*pte1)));
            pte0 = (PTE *)pt0_va + vpn0;
        }else{
            ptr_t pt0_pa = (*pte1 >> 10) << 12;
            uintptr_t pt0_va = pa2kva(pt0_pa);

            pte0 = (PTE *)pt0_va + vpn0;
        }


        if(((*pte0) & 0x1) != 0){
            return 0; // the va has been mapped
        }
        uintptr_t final_pa = phy_addr;
        set_pfn(pte0, final_pa >> NORMAL_PAGE_SHIFT);

        set_attribute(pte0, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |_PAGE_USER| _PAGE_ACCESSED | _PAGE_DIRTY);


        //if the 1st access, clear the page
        if(shm[shm_idx].map_num == 1 && shm[shm_idx].used_flag == 0){
            uint64_t kva = pa2kva(phy_addr);
            for (int i = 0; i < 512; i++){
                *((uint64_t*)kva + i) = 0;
            }
            shm[shm_idx].used_flag = 1;
        }


        return user_addr;
        
    }

    void shm_page_dt(uintptr_t addr)
    {
        // TODO [P4-task4] shm_page_dt:
        uint64_t cpu_id = get_current_cpu_id();
        int shm_idx = 0;
        int i = 0;
        int j = 0;
        int flag = 0;
        for (i = 0; i < MOST_SHARE_PAGE; i++){
            for ( j = 0; j < 10; j++){
                if(addr == shm[i].vaddr[j]){
                    shm_idx = i;
                    flag = 1;
                    break;
                }
            }
            if(flag)
                break;
        }

        // if(i == MOST_SHARE_PAGE){
        //     printk("error: the addr isn't used for share memory.");
        // }

        //cancel the addr map    
        PTE *pte = (PTE *)walk_pgdir(addr, pa2kva(next_running[cpu_id]->pgdir));

        if(pte == 0){
            printk("error: the addr doesn't been mapped.");
        }

        *pte = 0;

        //the shm_t 
        shm[shm_idx].vaddr[j] = NULL;
        shm[shm_idx].map_num--;

        if(shm[shm_idx].map_num == 0){
            shm[shm_idx].used_flag = 0;
        }
    }

