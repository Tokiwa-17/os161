#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <synch.h>
#include <current.h>
#include <proc.h>
#include <spl.h>

/* Place your page table functions here */
// static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

uint32_t hash_func(struct addrspace *as, vaddr_t faultaddr)
{
    uint32_t index;
    index = (((uint32_t) as) ^ (faultaddr >> 12)) % hpt_size;
    return index;
}

bool hpt_insert(struct addrspace *as, vaddr_t hi, paddr_t lo)
{
    uint32_t idx = hash_func(as, hi);
    if (hpt[idx].entryLO == 0) 
    {
        hpt[idx].entryLO = lo;
        hpt[idx].entryHI = hi;
        hpt[idx].as = as;
        return false;
    }
    while (hpt[idx].next != -1)
    {
        idx = hpt[idx].next;
    }
    for (uint32_t new_idx = 0; new_idx < hpt_size; new_idx++)
    {
        if (hpt[new_idx].entryHI == 0 && hpt[new_idx].entryLO == 0 && hpt[new_idx].next == -1)
        {
            hpt[new_idx].entryHI = hi;
            hpt[new_idx].entryLO = lo;
            hpt[new_idx].as = as;
            hpt[idx].next = new_idx;
            return false;
        }
    }
    return true;
}

void 
vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
    *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
    as_count = 0;
    hpt_lock = lock_create("hpt_lock");
    unsigned long ram_size = ram_getsize();
    frame_table_size = ram_size / PAGE_SIZE;
    hpt_size = frame_table_size << 1;
    hpt = kmalloc(hpt_size * sizeof(struct hash_page_table));
    frame_table_status = kmalloc(frame_table_size);
    frame_table_start = 1 + ram_getfirstfree() / PAGE_SIZE;

    lock_acquire(hpt_lock);
    for (uint32_t i = 0; i < hpt_size; i++) {
        hpt[i].entryHI = 0;
        hpt[i].entryLO = 0;
        hpt[i].as      = 0;
        hpt[i].next    = -1;
    }
    lock_release(hpt_lock);
    for (uint32_t i = 0; i < frame_table_size; i++)
    {
        if (i < frame_table_start) 
        {
            frame_table_status[i] = false;
        } else if (i >= frame_table_start && i < frame_table_size)
        {
            frame_table_status[i] = true;   
        }
    }
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void) faulttype;
    (void) faultaddress;
    return 0;

}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

