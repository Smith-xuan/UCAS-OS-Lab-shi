#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_TASK];
pid_t thread_idx = 0;
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

pcb_t * volatile next_running = &pid0_pcb;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs


    // TODO: [p2-task1] Modify the current_running pointer.
    
    current_running = next_running;
    check_sleeping();
    if (current_running->pid != 0){
        if (current_running->status == TASK_READY){
            list_add_tail(&(current_running->list), &ready_queue);
        }
    }
    else{
        current_running = &pid0_pcb;
    }

    if(!list_empty(&ready_queue)){
        pcb_t * temp_next_running = ready_queue.next;
        next_running = (pcb_t *)((uint64_t)(temp_next_running) - 2*sizeof(reg_t));
        list_del(&(next_running->list));
    }
    else{
        next_running = &pid0_pcb;
    }
    
//    next_running->status = TASK_RUNNING;

/*    if (pcb[process_id-1].status == TASK_READY){
        current_running = &pcb[process_id-1];
        next_running = pcb[process_id-1].list.next;
        process_id++;
    }*/
    // TODO: [p2-task1] switch_to current_running
    switch_to(current_running, next_running);

    
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    next_running->status = TASK_BLOCKED;
    // 2. set the wake up time for the blocked task
    next_running->wakeup_time = get_ticks() + sleep_time * get_time_base();
    list_del(&(next_running->list));
    list_add_tail(&(next_running->list), &sleep_queue);
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add_tail(pcb_node,queue);                                      //add pcb_node to block queue
    next_running -> status = TASK_BLOCKED;                           //set the status to TAKS_BLOCKED
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_t *p = (pcb_t *)((uint64_t)(pcb_node->next) - 2*sizeof(reg_t)); //get the block_queue'head
    list_del(pcb_node->next);                                           //del the head pcb
    p->status = TASK_READY;                                             //set the pcb TASK_READY
    list_add_tail(&(p->list), &(ready_queue));                          //add to the ready_queue
}

void thread_create(pid_t *thread, void(*thread_func)(void *), void *argv)
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