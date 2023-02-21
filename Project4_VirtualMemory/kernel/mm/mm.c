#include <os/mm.h>
#include <os/sched.h>
#include <pgtable.h>
#include <os/smp.h>



static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t kmallocCurr = KMALLOC_BASE;

#define SHARE_P_MEM_BASE 0x56000000
#define SHARE_U_MEM_BASE 0xf60000000
#define MOST_SHARE_PAGE 20
static ptr_t sharepMemCurr = SHARE_P_MEM_BASE;

shm_t shm[MOST_SHARE_PAGE];
// static ptr_t userMemCurr = FREEMEM_USER;

// ptr_t allocKernelPage(int numPage)
// {
//     // align PAGE_SIZE
//     ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
//     kernMemCurr = ret + numPage * PAGE_SIZE;
//     return ret;
// }

// ptr_t allocUserPage(int numPage)
// {
//     // align PAGE_SIZE
//     ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
//     userMemCurr = ret + numPage * PAGE_SIZE;
//     return ret;
// }

static LIST_HEAD(FreePageList);

ptr_t allocPage()
{
    if(!list_empty(&FreePageList)){
        page_t *fp = (page_t *)((uint64_t)(FreePageList.next) - 2*sizeof(uint64_t));
        uint64_t kva = pa2kva(fp->pa);
        for (int i = 0; i < 512; i++){
            *((uint64_t*)kva + i) = 0;
        }
        list_del(FreePageList.next);
        return kva;
    }else{
        ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
        kernMemCurr = ret + PAGE_SIZE;
        for (int i = 0; i < 512; i++){
            *((uint64_t*)kernMemCurr + i) = 0;
        }
        return ret;
    }

    // align PAGE_SIZE

}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    page_t *fp = (page_t *)kmalloc(sizeof(page_t));
    fp->pa = baseAddr;
    list_add_tail(&(fp->list), &FreePageList);
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    ptr_t ret = ROUND(kmallocCurr, 4);
    kmallocCurr = ret + size;
    return (void *)ret;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
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


uintptr_t walk_pgdir(uintptr_t va, uintptr_t pgdir)
{
    va = va & VA_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS +PPN_BITS)) & ~(~0 << PPN_BITS);

    PTE *pte2 = (PTE *)pgdir + vpn2;
    PTE *pte1 = NULL;
    PTE *pte0 = NULL;

    if(((*pte2) & 0x1) == 0){
        return 0;
    }
    pte1 = (PTE*)pa2kva(((*pte2) >> 10) << 12) + vpn1;

    if(((*pte1) & 0x1) == 0){
        return 0;
    }
    pte0 = (PTE*)pa2kva(((*pte1) >> 10) << 12) + vpn0;

    if(((*pte0) & 0x1) == 0){
        return 0;
    }

    return pte0;
}


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


