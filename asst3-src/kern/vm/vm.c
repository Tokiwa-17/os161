#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>


/* Place your page table functions here */
// static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
    int ramsize = ram_getsize();
    int npages = ramsize / PAGE_SIZE;
    int hpt_size = npages << 1;
    hpt = (struct hpt_entry *) alloc_kpages(hpt_size);
    for (int i = 0; i < hpt_size; i++) {
        hpt[i].pte = NULL;
        hpt[i].next = NULL;
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

