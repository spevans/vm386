/* fault.c -- Page-fault handler.
   John Harper. */

#include <vmm/types.h>
#include <vmm/page.h>
#include <vmm/traps.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/tasks.h>

/* Code to handle a page fault. */
void
page_exception_handler(struct trap_regs *regs)
{
    static int nest_count;
    u_long page_phys_addr = get_cr2() & PAGE_MASK;
    u_long page_offset = get_cr2() & PAGE_OFFSET_MASK;
    u_long pte;
    if(++nest_count > 1)
    {
	kprintf("Nested page fault!\n");
	cli();hlt();
    }
    DB(("page_exception_handler: %%cr2=%#0x ec=%#0x %%eip=%#0x\n",
	get_cr2(), regs->error_code, regs->eip));
    pte = get_pte(current_task->page_dir, page_phys_addr);
    DB(("p_e_h: pte=%#0x\n", pte));
    if(regs->error_code & PF_ERROR_PROTECTION)
    {
	/* Page-level protection violation. */
	if(regs->error_code & PF_ERROR_USER)
	{
	    /* From a user-level task. */
	    if(!(((pte & (PTE_USER | PTE_READ_WRITE)) == PTE_USER)
		 && current_task->pfl_handler
		 && current_task->pfl_handler(regs, page_phys_addr
					      | page_offset)))
	    {
		kprintf("User level page protection violation; addr=%#0x ec=%#0x\n",
			page_phys_addr | page_offset, regs->error_code);
		dump_regs(regs, TRUE);
	    }
	}
	else
	{
	    /* From a level zero task. Eek. Actually this shouldn't ever
	       happen unless we start setting the WP bit on 486s.. */
	    kprintf("Level 0 page protection violation; addr=%#0x ec=%#0x\n",
		    page_phys_addr | page_offset, regs->error_code);
	    dump_regs(regs, TRUE);
	}
    }
    else
    {
	if(((regs->error_code & PF_ERROR_USER) == 0)
	   && (page_phys_addr >= KERNEL_BASE_ADDR)
	   && ((page_phys_addr < TO_LINEAR(0x1000))
	       || (page_phys_addr >= TO_LINEAR(kernel_brk))))
	{
	    /* Someone in level 0 is accessing a page they shouldn't! */
	    kprintf("Level 0 invalid not-present-page access; addr=%#0x ec=%#0x\n",
		    page_phys_addr | page_offset, regs->error_code);
	    dump_regs(regs, TRUE);
	}
	else
	{
	    /* Caused by an access of a not-present page; if the task's
	       pfl_handler() hook exists call it, else get an unused page
	       and map it into the hole. */
	    if(current_task->pfl_handler)
	    {
		current_task->pfl_handler(regs, page_phys_addr | page_offset);
	    }
	    else
	    {
		page *new = alloc_page();
		map_page(current_task->page_dir, new, page_phys_addr,
			 (pte & (PTE_USER | PTE_READ_WRITE | PTE_FREEABLE))
			 | PTE_PRESENT);
		DB(("p_e_f: mapped in not-present page at %#x, page=%#x\n",
		    page_phys_addr | page_offset, new));
	    }
	}
    }
    nest_count--;
}
