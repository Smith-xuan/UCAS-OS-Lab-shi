#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <os/mm.h>
#include <os/smp.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];
int swap_block = 10000;
int print_location = 10;

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`

    handler_t *table = (scause >> 63)? irq_table: exc_table;
    scause = scause << 1;
    scause = scause >> 1;              // clear the 64th bit
    table[scause](regs, stval, scause);// invoke the function

}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    reset_timer();
}

void reset_timer()
{
    bios_set_timer(get_ticks() + get_time_base() / 100);
    do_scheduler();
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i = 0; i < EXCC_COUNT; i++){
        exc_table[i] = &(handle_other);
    }
    exc_table[EXCC_SYSCALL] = &handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT] = &handle_inst_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT] = &handle_load_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = &handle_store_page_fault;
    

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int i = 0; i < IRQC_COUNT; i++){
        irq_table[i] = &(handle_other);
    }
    irq_table[IRQC_S_TIMER] = &handle_irq_timer;
    

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    //setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}

void store_to_disk(int block_id, int block_num, uintptr_t pa)
{
    bios_sdwrite(pa, block_num, block_id);
    screen_move_cursor(1, print_location++);
    printk("store to SD successfully");
    screen_reflush();
}

void load_to_mem(int block_id, int block_num, uintptr_t pa)
{
    bios_sdread(pa, block_num, block_id);
    screen_move_cursor(1, print_location++);
    printk("load to SD successfully");
    screen_reflush();
}

int do_swap(pcb_t *pcb)
{
    if(list_empty(&(pcb->plist))){
        return 0;
    }
    list_node_t *p1 = pcb->plist.next;
    page_t *to_swap_page = NULL;


    while(p1 != &(pcb->plist)){
        to_swap_page = (page_t *)((uint64_t)(p1) - 2*sizeof(uint64_t));

        uintptr_t *pte = walk_pgdir(to_swap_page->va, pa2kva(pcb->pgdir));

        // if(pte == 0){
        //     return 0;//the page hadn't been mapped
        // }


        //not in disk, swap it(because of the list queue,it's a FIFO)
        if(!to_swap_page->in_disk){
            store_to_disk(swap_block, 8, to_swap_page->pa);
            //update the page_t
            to_swap_page->block_id = swap_block;
            swap_block += 8;
            to_swap_page->in_disk = 1;
            to_swap_page->pte = pte;
            *pte &= ~((uint64_t)0x1);
            pcb->pnum --;
            return 1;
        }

        p1 = p1->next;
    }

    printk("no page that can be stored in disk");
    return 0;
}


int do_unswap(pcb_t *pcb, uintptr_t va)
{
    list_node_t *p1 = pcb->plist.next;
    page_t *to_unswap_page = NULL;

    while(p1 != &(pcb->plist)){
        to_unswap_page = (page_t *)((uint64_t)(p1) - 2*sizeof(uint64_t));


        if(va == to_unswap_page->va && to_unswap_page->in_disk == 1){
            load_to_mem(to_unswap_page->block_id, 8, to_unswap_page->pa);
            //update the page_t
            to_unswap_page->in_disk = 0;
            uintptr_t *pte = to_unswap_page->pte;
            set_attribute(pte, 0x1);
            pcb->pnum ++;

            list_del(p1);
            list_add_tail(p1, &(pcb->plist));
            return 1;
        }

        p1 = p1->next;
    }
    //don't find
    return 0;
}

void handle_inst_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t cpu_id = get_current_cpu_id();
    uintptr_t pte = walk_pgdir(stval, pa2kva(next_running[cpu_id]->pgdir));

    if(pte != 0){  //had been mapped. maybe the A&D flag bits
        set_attribute(pte, _PAGE_ACCESSED | _PAGE_DIRTY);
        local_flush_tlb_all();
        return;
    }else{
        while(1) ;
    }
}


void handle_load_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t cpu_id = get_current_cpu_id();
    uintptr_t pte = walk_pgdir(stval, pa2kva(next_running[cpu_id]->pgdir));

    if(pte != 0){  //maybe the A&D
        set_attribute(pte, _PAGE_ACCESSED);
        local_flush_tlb_all();
        return;
    }
    //the wrong addr page is in disk
    if(do_unswap(next_running[cpu_id], (stval >> 12) << 12)){
        if(next_running[cpu_id]->pnum >= 3){
            do_swap(next_running[cpu_id]);
        }
        local_flush_tlb_all();
        return;
    }
    //a process can have 5 pages at most
    if(next_running[cpu_id]->pnum >= 3){
        do_swap(next_running[cpu_id]);
    }

    //alloc a new page and map it
    uintptr_t page_new;
    page_new = alloc_page_helper(stval, pa2kva(next_running[cpu_id]->pgdir));
    
    //alloc a page_t for the new_page
    page_t *p = (page_t *)kmalloc(sizeof(page_t));
    p->in_disk = 0;
    p->pa = kva2pa(page_new);
    p->va = (stval >> 12) << 12;
    
    //add the page_t into the current process's plist
    list_add_tail(&(p->list), &(next_running[cpu_id]->plist));
    next_running[cpu_id]->pnum++;
    local_flush_tlb_all();
}


void handle_store_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t cpu_id = get_current_cpu_id();
    uintptr_t pte = walk_pgdir(stval, pa2kva(next_running[cpu_id]->pgdir));

    if(pte != 0){  //maybe the A&D
        set_attribute(pte, _PAGE_ACCESSED | _PAGE_DIRTY);
        local_flush_tlb_all();
        return;
    }
    //the wrong addr page is in disk
    if(do_unswap(next_running[cpu_id], (stval >> 12) << 12)){
        if(next_running[cpu_id]->pnum >= 3){
            do_swap(next_running[cpu_id]);
        }
        local_flush_tlb_all();
        return;
    }
    //a process can have 5 pages at most
    if(next_running[cpu_id]->pnum >= 3){
        do_swap(next_running[cpu_id]);
    }

    //alloc a new page and map it
    uintptr_t page_new;
    page_new = alloc_page_helper(stval, pa2kva(next_running[cpu_id]->pgdir));
    
    //alloc a page_t for the new_page
    page_t *p = (page_t *)kmalloc(sizeof(page_t));
    p->in_disk = 0;
    p->pa = kva2pa(page_new);
    p->va = (stval >> 12) << 12;
    
    //add the page_t into the current process's plist
    list_add_tail(&(p->list), &(next_running[cpu_id]->plist));
    next_running[cpu_id]->pnum++;
    local_flush_tlb_all();
}