#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/smp.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/string.h>
#include <os/task.h>

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_TASK];
pid_t thread_idx = 0;
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

const ptr_t pid0_stack_slave = INIT_KERNEL_STACK_SLAVE + PAGE_SIZE;
pcb_t pid0_pcb_slave = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack_slave,
    .user_sp = (ptr_t)pid0_stack_slave
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t* volatile current_running[2];
pcb_t* volatile next_running[2];


/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    uint64_t cpu_id = get_current_cpu_id();


    // TODO: [p2-task1] Modify the current_running pointer.
    
    current_running[cpu_id] = next_running[cpu_id];
    check_sleeping();
    while(list_empty(&ready_queue) && !list_empty(&sleep_queue) && (next_running[cpu_id]->status!=TASK_RUNNING)){
        check_sleeping();
    }
    if (current_running[cpu_id]->pid != 0){
        if (current_running[cpu_id]->status == TASK_READY || current_running[cpu_id]->status == TASK_RUNNING){
            current_running[cpu_id]->status = TASK_READY;
            list_del(&(current_running[cpu_id]->list));
            list_add_tail(&(current_running[cpu_id]->list), &ready_queue);
        }
    }else if(current_running[cpu_id]->kernel_sp == pid0_stack){
        current_running[cpu_id] = &pid0_pcb;
    }else if(current_running[cpu_id]->kernel_sp == pid0_stack_slave){
        current_running[cpu_id] = &pid0_pcb_slave;
    }

    // if(!list_empty(&ready_queue)){
    //     pcb_t * temp_next_running = ready_queue.next;
    //     next_running[cpu_id] = (pcb_t *)((uint64_t)(temp_next_running) - 4*sizeof(reg_t));
    //     list_del(&(next_running[cpu_id]->list));
    // }
    // else{
    //     next_running[cpu_id] = &pid0_pcb;
    // }

    list_node_t *p1;
    pcb_t *p2;
    if(!list_empty(&ready_queue)){
        p1 = ready_queue.next;
        p2 = (pcb_t *)((uint64_t)(p1) - 4*sizeof(reg_t));
        while((p2->type != 0) && !((p2->type == 1) && (cpu_id == 0)) && !((p2->type == 2) && (cpu_id == 1)) && (p1 != &ready_queue)){ //taskset core
            p1 = p1->next;
            p2 = (pcb_t *)((uint64_t)(p1) - 4*sizeof(reg_t));
        }
        if(p1 == &ready_queue){
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



    next_running[cpu_id]->cpu_id = cpu_id; // mark current process running on which core
//    next_running->status = TASK_RUNNING;

/*    if (pcb[process_id-1].status == TASK_READY){
        current_running = &pcb[process_id-1];
        next_running = pcb[process_id-1].list.next;
        process_id++;
    }*/
    // TODO: [p2-task1] switch_to current_running
    next_running[cpu_id]->status = TASK_RUNNING;
    switch_to(current_running[cpu_id], next_running[cpu_id]);

    
}

void do_sleep(uint32_t sleep_time)
{
    uint64_t cpu_id = get_current_cpu_id();
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    next_running[cpu_id]->status = TASK_BLOCKED;
    // 2. set the wake up time for the blocked task
    next_running[cpu_id]->wakeup_time = get_ticks() + sleep_time * get_time_base();
    list_del(&(next_running[cpu_id]->list));
    list_add_tail(&(next_running[cpu_id]->list), &sleep_queue);
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    uint64_t cpu_id = get_current_cpu_id();
    list_add_tail(pcb_node,queue);                                      //add pcb_node to block queue
    next_running[cpu_id] -> status = TASK_BLOCKED;                           //set the status to TAKS_BLOCKED
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_t *p = (pcb_t *)((uint64_t)(pcb_node->next) - 4*sizeof(reg_t)); //get the block_queue'head
    list_del(pcb_node->next);                                           //del the head pcb
    p->status = TASK_READY;                                             //set the pcb TASK_READY
    list_add_tail(&(p->list), &(ready_queue));                          //add to the ready_queue
}

void do_process_show()
{
    int n = 0;
    printk("[PROCESS TABLE]\n");
    for(int i = 0; i < process_id; i++){
        if(pcb[i].status == TASK_RUNNING && pcb[i].pid !=0){
            printk("[%d] PID : %d STATUS : RUNNING TYPE : %d RUNNING ON CORE_ID : %d\n", n,pcb[i].pid,pcb[i].type,pcb[i].cpu_id);
            n++;
        }else if(pcb[i].status == TASK_READY && pcb[i].pid !=0){
            printk("[%d] PID : %d STATUS : READY TYPE : %d \n", n,pcb[i].pid,pcb[i].type);
            n++;
        }else if(pcb[i].status == TASK_BLOCKED && pcb[i].pid != 0){
            printk("[%d] PID : %d STATUS : BLOCKED TYPE : %d \n", n,pcb[i].pid,pcb[i].type);
            n++;
        }
    }
}

int do_kill(pid_t pid)
{
    pcb_t *pcb_kill;

    int i = 0;
    for (i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid){
            break;
        }
    }
    //shell or can not find the process
    if(i == NUM_MAX_TASK || pid == 0 || pid == 1)
        return 0;
    
    pcb_kill = &pcb[i];
    //delete the killing pcb from its queue
    if(pcb_kill->status == TASK_READY || pcb_kill->status == TASK_BLOCKED){
        list_del(&(pcb_kill->list));
    }
    //release the lock
    for (i = 0; i < pcb_kill->lock_num; i++){
        if(!list_empty(&(pcb_kill->lock[i]->block_queue))){
            pcb_t *p = (pcb_t *)((uint64_t)((&(pcb_kill->lock[i]->block_queue))->next) - 4*sizeof(reg_t)); //get the block_queue'head
            list_del((&(pcb_kill->lock[i]->block_queue))->next);                                           //del the head pcb
            p->status = TASK_READY;                                             //set the pcb TASK_READY
            list_add_tail(&(p->list), &(ready_queue));                          //add to the ready_queue
        }else{
            pcb_kill->lock[i]->lock.status = UNLOCKED;
        }
        pcb_kill->lock[i] = NULL;
    }
    //delete the lists in wait_list
    while(!list_empty(&(pcb_kill->wait_list))){
        pcb_t *p1;
        p1 = (pcb_t *)((uint64_t)(pcb_kill->wait_list.next) - 4*sizeof(reg_t));
        if(p1->status == TASK_BLOCKED){
            p1->status = TASK_READY;
            list_del(&(p1->list));
            list_add_tail(&(p1->list), &ready_queue);
        }
    }
    
    pcb_kill->type = 0;
    pcb_kill->status = TASK_EXITED;
    pcb_kill->pid = 0;

    do_scheduler();

    return 1;
    
}

void do_exit(void)
{
    uint64_t cpu_id = get_current_cpu_id();
    pcb_t *pcb_exit = next_running[cpu_id];

    //release the lock
    for (int i = 0; i < pcb_exit->lock_num; i++){
        if(!list_empty(&(pcb_exit->lock[i]->block_queue))){
            pcb_t *p = (pcb_t *)((uint64_t)((&(pcb_exit->lock[i]->block_queue))->next) - 4*sizeof(reg_t)); //get the block_queue'head
            list_del((&(pcb_exit->lock[i]->block_queue))->next);                                           //del the head pcb
            p->status = TASK_READY;                                             //set the pcb TASK_READY
            list_add_tail(&(p->list), &(ready_queue));                          //add to the ready_queue
        }else{
            pcb_exit->lock[i]->lock.status = UNLOCKED;
        }
        pcb_exit->lock[i] = NULL;
    }
    //delete the lists in wait_list
    while(!list_empty(&(pcb_exit->wait_list))){
        pcb_t *p1;
        p1 = (pcb_t *)((uint64_t)(pcb_exit->wait_list.next) - 4*sizeof(reg_t));
        if(p1->status == TASK_BLOCKED){
            p1->status = TASK_READY;
            list_del(&(p1->list));
            list_add_tail(&(p1->list), &ready_queue);
        }
    }

    pcb_exit->type = 0;
    pcb_exit->status = TASK_EXITED;
    pcb_exit->pid = 0;

    do_scheduler();

}

int do_waitpid(pid_t pid)
{
    uint64_t cpu_id = get_current_cpu_id();

    pcb_t *pcb_wait;

    int i = 0;
    for (i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid){
            break;
        }
    }
    //shell or can not find the process
    if(i == NUM_MAX_TASK || pid == 0 || pid == 1)
        return 0;
    
    pcb_wait = &pcb[i];

    //block current process in the target pcb's wait_list
    list_add_tail(&(next_running[cpu_id]->list),&(pcb_wait->wait_list));
    next_running[cpu_id]->status = TASK_BLOCKED;

    do_scheduler();
    return 1;
}

pid_t do_getpid()
{
    uint64_t cpu_id = get_current_cpu_id();

    pid_t pid_get = next_running[cpu_id]->pid;
    return pid_get;
}

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

pid_t do_taskset(char *name, int argc, char **argv, int type)
{
    int taskpid = do_exec(name, argc, argv);
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == taskpid){
            pcb[i].type = type;
            break;
        }
    }
    if(type == 1){
        printk("type1");
    }

    printk("change type succssfully %d\n",taskpid);
    volatile char *flagflag = (void *) 0x59000000;
    *flagflag = 1;

    return taskpid;
}

void do_tasksetrun(pid_t pid, int type)
{
    int i = 0;
    for (i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid){
            pcb[i].type = type;
            printk("taskset succssfully, pid = %d\n", pid);
            break;
        }
    }
    if(i == NUM_MAX_TASK){
        printk("cant find the process with the pid %d\n", pid);
    }
}

void thread_create(pid_t *thread, void(*thread_func)(void *), void *argv)
{
    uint64_t cpu_id = get_current_cpu_id();

    thread_idx++;
    next_running[cpu_id]->thread_num++;
    pcb_t *new_tcb = &tcb[thread_idx];
    new_tcb->thread_idx = thread_idx;
    new_tcb->pid = next_running[cpu_id]->pid;
    new_tcb->tid = next_running[cpu_id]->thread_num;
    new_tcb->status = TASK_READY;

    list_add_tail(&new_tcb->list,&ready_queue);

    uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    uint64_t user_stack = allocUserPage(1) + PAGE_SIZE;
    uint64_t entry = (uint64_t)(*thread_func);
    init_pcb_stack(kernel_stack,user_stack,entry,new_tcb); 

    regs_context_t *pt_regs =
        (regs_context_t *)((ptr_t)(new_tcb->kernel_sp) + sizeof(switchto_context_t));

    pt_regs->regs[10] = argv;
}


// list API
void init_list_head(list_head *list)
{
    list->next = list;
    list->prev = list;
}

void _list_add(list_node_t *node, list_node_t *prev, list_node_t *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

void _list_del(list_node_t *prev, list_node_t *next)
{
    next->prev = prev;
    prev->next = next;
}

void list_del(list_node_t *entry)
{
    if (entry->prev != NULL && entry->next != NULL) {
        _list_del(entry->prev, entry->next);
        entry->prev = NULL;
        entry->next = NULL;
    }
}

void list_add(list_node_t *node, list_node_t *head)
{
    _list_add(node, head, head->next);
}

void list_add_tail(list_node_t *node, list_node_t *head)
{
    _list_add(node, head->prev, head);
}

int list_empty(const list_head *head)
{
    return head->next == head;
}

void free_node(list_node_t *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = NULL;
    node->prev = NULL;
}