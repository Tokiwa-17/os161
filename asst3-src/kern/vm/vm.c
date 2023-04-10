#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

static struct frame_table_entry *frame_table;
static paddr_t frame_top, free_frame;
/* Place your page table functions here */
// static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
void
frame_table_bootstrap(void) 
{   
    paddr_t firstpaddr = 0, lastpaddr;
    lastpaddr = ram_getsize();
    unsigned int frame_num = (lastpaddr - firstpaddr) / PAGE_SIZE * 2;
    unsigned int frame_table_size = frame_num * sizeof(struct frame_table_entry);
    frame_table_size = ROUNDUP(frame_table_size, PAGE_SIZE);
    unsigned int entry_num = frame_table_size / PAGE_SIZE;
    KASSERT((frame_table_size & PAGE_FRAME) == frame_table_size);

    frame_top = firstpaddr;
    free_frame = firstpaddr + frame_table_size;
    KASSERT(free_frame < lastpaddr);

    frame_table = (struct frame_table_entry *) PADDR_TO_KVADDR(firstpaddr);
    struct frame_table_entry*  p = frame_table;

    paddr_t paddr;
    for (unsigned int i = 0; i < frame_num - 1; i++)
    {
        if (i < entry_num)
        {
            p -> next_free_frame = 0; // set to allocated
            p += 1;
        } else {
            paddr = frame_top + (i + 1) * PAGE_SIZE;
            p -> next_free_frame = paddr;
            p += 1;
        }
    }
}


void 
vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
    frame_table_bootstrap();
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

