#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <synch.h>

/* Place your page table functions here */
// static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct lock* hpt_lock;

void 
vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
    *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
    //  frame_table_bootstrap();   
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

    panic("vm_fault hasn't been written yet\n");

    return EFAULT;
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

