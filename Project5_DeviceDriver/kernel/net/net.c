#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    int cpu_id = get_current_cpu_id();
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // while(!e1000_transmit(txpacket,length)){
    //     ;
    // }
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    while(!e1000_transmit(txpacket,length)){
        next_running[cpu_id]->status = TASK_BLOCKED;
        list_del(&(next_running[cpu_id]->list));
        list_add_tail(&(next_running[cpu_id]->list), &send_block_queue);
        do_scheduler();
    }
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return length;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    int cpu_id = get_current_cpu_id();
    int all_length = 0;
    int length = 0;
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // for (int i = 0; i < pkt_num; i++){
    //     while((length = e1000_poll((uint32_t *)((uint64_t)(rxbuffer)+(all_length)))) == 0){
    //         ;
    //     } 
    //     all_length += length;
    //     pkt_lens[i] = length;
    // }
    

    // TODO: [p5-task3] Call do_block when there is no packet on the way
    for (int i = 0; i < pkt_num; i++){
        while((length = e1000_poll((uint32_t *)((uint64_t)(rxbuffer)+(all_length)))) == 0){
            next_running[cpu_id]->status = TASK_BLOCKED;
            list_del(&(next_running[cpu_id]->list));
            list_add_tail(&(next_running[cpu_id]->list), &recv_block_queue);
            do_scheduler();
        } 
        all_length += length;
        pkt_lens[i] = length;
    }
    return all_length;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
}

int check_net_send(void)
{
    pcb_t *p_now;
    list_node_t *node_now, *node_next;

    node_now = send_block_queue.next;
    while (!list_empty(&send_block_queue) && node_now != &send_block_queue){ //read the holy queue only once
        uint32_t tail;
        struct e1000_tx_desc *desc;

        tail = e1000_read_reg(e1000, E1000_TDT);
        desc = &tx_desc_array[tail];

        node_next = node_now->next;
        p_now = (pcb_t *)((uint64_t)node_now - 4*sizeof(reg_t));
        if (desc->status & E1000_TXD_STAT_DD){
            p_now->status = TASK_READY;
            list_del(&(p_now->list));
            list_add_tail(&(p_now->list), &ready_queue);
        }
        node_now = node_next;
    }
}

int check_net_recv(void)
{
    pcb_t *p_now;
    list_node_t *node_now, *node_next;

    node_now = recv_block_queue.next;
    while (!list_empty(&recv_block_queue) && node_now != &recv_block_queue){ //read the holy queue only once
        uint32_t tail;
        struct e1000_rx_desc *desc;

        tail = (e1000_read_reg(e1000, E1000_RDT)+1) % RXDESCS;
        desc = &(rx_desc_array[tail]);
        
        node_next = node_now->next;
        p_now = (pcb_t *)((uint64_t)node_now - 4*sizeof(reg_t));
        if (desc->status & E1000_TXD_STAT_DD){
            p_now->status = TASK_READY;
            list_del(&(p_now->list));
            list_add_tail(&(p_now->list), &ready_queue);
        }
        node_now = node_next;
    }
}