#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    pcb_t *p_now;
    list_node_t *node_now, *node_next;

    node_now = sleep_queue.next;
    while (!list_empty(&sleep_queue) && node_now != &sleep_queue){ //read the holy queue only once
        node_next = node_now->next;
        p_now = (pcb_t *)((uint64_t)node_now - 2*sizeof(reg_t));
        if (p_now->wakeup_time <= get_ticks()){
            p_now->status = TASK_READY;
            list_del(&(p_now->list));
            list_add_tail(&(p_now->list), &ready_queue);
        }
        node_now = node_next;
    }
    
}