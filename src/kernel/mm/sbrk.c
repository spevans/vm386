/* sbrk.c -- Kernel sbrk() function.
   John Harper. */

#include <vmm/page.h>
#include <vmm/io.h>

/* This is initialised by init_mm(). */
u_char *kernel_brk;

/* Add DELTA to the kernel's break address, returning the old break
   address. Note that DELTA may be negative if you want. Returns -1
   if no more memory is available. */
void *
kernel_sbrk(long delta)
{
#ifndef SBRK_DOESNT_ALLOC
    u_char *old = kernel_brk, *ptr;
    u_long flags;
    save_flags(flags);
    cli();
    kernel_brk += round_to(delta, 4);
    if((u_long)kernel_brk < (PHYS_MAP_ADDR - KERNEL_BASE_ADDR))
    {
	ptr = (u_char *)round_to((u_long)old, PAGE_SIZE);
	while(ptr < kernel_brk)
	{
	    page *p = alloc_page();
	    if(p == NULL)
		goto error;
	    map_page(logical_kernel_pd, p, TO_LINEAR(ptr), PTE_PRESENT);
	    ptr += PAGE_SIZE;
	}
	load_flags(flags);
	return old;
    }
 error:
    kernel_brk = old;
    load_flags(flags);
    return (void *)-1;
#else
    /* Don't need to map in any pages or anything; let the page-fault-
       handler do that. Should really release any unneeded pages if
       DELTA is negative. */
    register void *ptr = kernel_brk;
    kernel_brk += round_to(delta, 4);
    if((u_long)kernel_brk < (PHYS_MAP_ADDR - KERNEL_BASE_ADDR))
	return ptr;
    kernel_brk = ptr;
    return (void *)-1;
#endif
}
