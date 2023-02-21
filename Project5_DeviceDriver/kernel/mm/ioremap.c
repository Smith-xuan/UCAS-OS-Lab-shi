#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t pgdir = 0xffffffc051000000;
    uintptr_t* rtaddr = NULL;
    int flag = 0;

    while(size > 0){
        uint64_t va = io_base;

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

        set_pfn(pte0, phys_addr >> NORMAL_PAGE_SHIFT);
        set_attribute(pte0, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

        if(!flag){
            flag = 1;
            rtaddr = io_base;
        }

        phys_addr += PAGE_SIZE;
        size -= PAGE_SIZE;
        io_base += PAGE_SIZE;
    }

    local_flush_tlb_all();
    return rtaddr;
}



void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
