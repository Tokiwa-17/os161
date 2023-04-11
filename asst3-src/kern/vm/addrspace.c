/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <synch.h>

static struct lock* hpt_lock;
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */
struct as_region *
create_region(vaddr_t v, size_t s, mode_t m, mode_t bm)
{
	struct as_region * reg = kmalloc(sizeof(struct as_region));
	if (reg == NULL) {
		return NULL;
	}
	reg -> vbase = v;
	reg -> size = s;
	reg -> mode = m;
	reg -> bk_mode = bm;
	reg -> next_region = NULL;
	return reg;
}

static
void 
free_kpages_frame(uint32_t frame) {
    free_kpages(PADDR_TO_KVADDR(frame << 12));
}

struct addrspace *
as_create(void)
{
	/*    as_create - create a new empty address space.
	* return NULL on out-of-memory error.*/
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}
	/*
	 * Initialize as needed.
	 */
	as_count += 1;
	as -> header = NULL;
	as -> id = as_count << 6;
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	/* as_copy   - create a new address space that is an exact copy of
	*  an old one. */
	struct addrspace *newas;
	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
	KASSERT(old -> header != NULL);
	newas -> header = create_region(old -> header -> vbase, old -> header -> size, old -> header -> mode, old -> header -> bk_mode);

	struct as_region *old_ptr = old -> header -> next_region;
	struct as_region *new_ptr = newas -> header;
	while(old_ptr != NULL) 
	{
		struct as_region *new_region = create_region(old_ptr -> vbase, old_ptr -> size, old_ptr -> mode, old_ptr -> bk_mode);
		if (new_region == NULL)
		{
			as_destroy(newas);
			return ENOMEM;
		}
		new_ptr -> next_region = new_region;
		new_ptr = new_ptr -> next_region;
		old_ptr = old_ptr -> next_region;
	}
	*ret = newas;
	uint32_t oldid = old -> id, newid = newas -> id;
	uint32_t new_cnt = 0, empty_cnt = 0, new_frame, old_frame, old_page;
	lock_acquire(hpt_lock);
	for (uint32_t i = 0; i < hpt_size; i++)
	{
		if ((hpt[i].entryHI & ~PAGE_FRAME) == oldid) // id = cnt << 6, 4k
			new_cnt ++;
		else if (hpt[i].entryHI == 0 && hpt[i].entryLO == 0 && hpt[i].next == -1)
			empty_cnt ++;
	}
	if (empty_cnt < new_cnt)
	{
		lock_release(hpt_lock);
		as_destroy(newas);
		return ENOMEM;
	}
	uint32_t available_cnt = 0;
	for (uint32_t i = frame_table_start; i < frame_table_size; i++)
	{
		if (frame_table_status[i])
			available_cnt ++;
	}
	if (available_cnt < new_cnt)
	{
		lock_release(hpt_lock);
		as_destroy(newas);
		return ENOMEM;
	}
	for (uint32_t i = 0; i < hpt_size; i++)
	{
		if (hpt[i].entryLO != 0 && (hpt[i].entryHI & ~PAGE_FRAME) == oldid)
		{ // 找到属于 old 的进程 且 非空的页表项
			//new_frame = alloc_kpages_frame() << 12; 
			vaddr_t temp = alloc_kpages(0);			// 计算新的页框号
			new_frame = temp ? (KVADDR_TO_PADDR(alloc_kpages(0)) << 12) : 0;
			old_page = hpt[i].entryHI & PAGE_FRAME; 
            old_frame = hpt[i].entryLO & PAGE_FRAME;
			memmove((void*)PADDR_TO_KVADDR(new_frame), (const void*)PADDR_TO_KVADDR(old_frame), PAGE_SIZE); // 复制旧页到新页
            hpt_insert(newas, old_page | newid, new_frame | TLBLO_VALID); 
			/* 6 ~ 12 位 页id, old_page | newid 虚拟内存页号不变*/
			/* 分配到新的页框， 标志位置为 VALID表示已被占用 */
		}
	}
	lock_release(hpt_lock);
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	struct as_region *cur = as -> header, *prev;
	while(cur != NULL) 
	{
		prev = cur;
		cur = cur -> next_region;
		kfree(prev);
	}

	uint32_t id = as -> id;
	uint32_t prev_idx;
	int next_delete;
	uint32_t addHI, addLO;
	struct addrspace* addas;

	lock_acquire(hpt_lock);
	for (uint32_t i = 0; i < hpt_size; i++)
	{
		if (hpt[i].entryLO != 0 && (hpt[i].entryHI & ~PAGE_FRAME) == id)
		{
			prev_idx = hash_func(as, hpt[i].entryHI);
			if (prev_idx != i) // 从冲突的hash链表中删除
			{
				while (prev_idx != i)
				{
					prev_idx = hpt[prev_idx].next;
				}
				hpt[prev_idx].next = -1;
			}
			next_delete = hpt[i].next;
			free_kpages_frame(hpt[i].entryLO >> 12);
			hpt[i].entryHI = 0;
            hpt[i].entryLO = 0;
            hpt[i].as = NULL;
            hpt[i].next = -1;
			while (next_delete != -1) {
                hpt[prev_idx].next = -1;
                prev_idx = next_delete;
                next_delete = hpt[next_delete].next;
                addHI = hpt[prev_idx].entryHI;
                hpt[prev_idx].entryHI = 0;
                addLO = hpt[prev_idx].entryLO;
                hpt[prev_idx].entryLO = 0;
                addas = hpt[prev_idx].as;
                hpt[prev_idx].as = NULL;
                hpt[prev_idx].next = -1;
                hpt_insert(addas, addHI, addLO);
            }
		}
	}
	lock_release(hpt_lock);
	kfree(as);
}

void
as_activate(void)
{
	 /* as_activate - make curproc's address space the one currently
      * "seen" by the processor. */
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	int spl;
	spl = splhigh();
	for (int i = 0; i < NUM_TLB; i++)
	{
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl); // restore
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS; /* Unimplemented */
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

