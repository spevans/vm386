/* page.c -- low-level page handling
   John Harper. */

#include <vmm/page.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/shell.h>

page_dir *logical_kernel_pd;		/* Initialised in init_mm() */


/* Page allocation. */

static page *page_free_list = NULL;
static int used_pages = 0, available_pages = 0;

/* Allocate one free page and return its *logical* address. If no free
   pages exist, return NULL. */
page *
alloc_page(void)
{
    int flags;
    page *p;
    save_flags(flags);
    cli();
    p = page_free_list;
    DB(("alloc_page: p=%p\n", page_free_list));
    if(p != NULL)
    {
	page_free_list = p->next_free;
	DB(("alloc_page: next_free=%p\n", page_free_list));
	used_pages++; available_pages--;
    }
    else
    {
	/* Eek. No more free pages :( */
	kprintf("alloc_page: No free pages!\n");
    }
    DB(("alloc_page: -> %p\n", p));
    load_flags(flags);
    return p;
}

/* Allocate N contiguous pages not crossing a 64K boundary. It also
   ensures that the area is below the 16M DMA limit. */
page *
alloc_pages_64(u_long n)
{
    page *x;
    u_long flags;
    save_flags(flags);
    cli();
    x = page_free_list;
    while(x != NULL)
    {
	if(((TO_PHYSICAL(x) & 0xffff) == 0)
	   && (TO_PHYSICAL(x) < 16*1024*1024))
	{
	    /* Found a page starting on a 64k boundary.
	       Now see if the other pages after it are free. */
	    page *top = x + n, *y = page_free_list;
	    u_long found = 1;
	    while(y != NULL)
	    {
		if((y > x) && (y < top))
		{
		    if(++found >= n)
		    {
			/* Yeah we found a space. Unlink it from the
			   free list. */
			page **ptr = &page_free_list;
			while(n != 0)
			{
			    if((*ptr >= x) && (*ptr < top))
			    {
				n--;
				*ptr = (*ptr)->next_free;
				used_pages++; available_pages--;
			    }
			    else
				ptr = &(*ptr)->next_free;
			}
			load_flags(flags);
			return x;
		    }
		}
		y = y->next_free;
	    }
	}
	x = x->next_free;
    }
    load_flags(flags);
    return NULL;
}

/* Free the page at logical address P. This function may be called from an
   IRQ handler or the normal kernel context. */
void
free_page(page *p)
{
    int flags;
    DB(("free_page: p=%p page_free_list=%p\n", p, page_free_list));
    save_flags(flags);
    cli();
    used_pages--; available_pages++;
    p->next_free = page_free_list;
    page_free_list = p;
    load_flags(flags);
    DB(("free_page: finished\n"));
}

/* Free N contiguous pages from P. */
void
free_pages(page *p, u_long n)
{
    while(n-- != 0)
    {
	free_page(p);
	p++;
    }
}

/* Add the chunk of physical memory from START to END as pages available
   for use. */
void
add_pages(u_long start, u_long end)
{
    int flags;
    save_flags(flags);
    cli();
    start = round_to(start, PAGE_SIZE);
    DB(("add_pages: start=%#x end=%#x\n", start, end));
    while(start < end)
    {
	page *p = TO_LOGICAL(start, page *);
	p->next_free = page_free_list;
	page_free_list = p;
	available_pages++;
	start += PAGE_SIZE;
    }
    DB(("add_pages: finished\n"));
    load_flags(flags);
}

u_long
free_page_count(void)
{
    return available_pages;
}


/* Page table & directory manipulation. */

/* Recursively free all page tables hanging from the page directory PD
   then free the PD page itself. Note PD is a *logical* address. This
   won't delete any kernel page tables. */
void
delete_page_dir(page_dir *pd)
{
    int i;
    for(i = 0; i < (PAGE_SIZE / 4); i++)
    {
	if((pd[i] & PTE_PRESENT)
	   && (pd[i] & PTE_FREEABLE))
	{
	    delete_page_table(TO_LOGICAL(PTE_GET_ADDR(pd[i]), page_table *));
	}
    }
    free_page((page *)pd);
}

/* Free all pages hanging from the page table PT, then free the PT page
   itself. This doesn't free kernel pages. */
void
delete_page_table(page_table *pt)
{
    int i;
    for(i = 0; i < (PAGE_SIZE / 4); i++)
    {
	if((pt[i] & PTE_PRESENT)
	   && (pt[i] & PTE_FREEABLE))
	{
	    free_page(TO_LOGICAL(PTE_GET_ADDR(pt[i]), page *));
	}
    }
    free_page((page *)pt);
}

/* Map the page PAGE into the page directory PD, so that it appears at
   linear address ADDR. FLAGS are the values to OR into the pte.
   Note that PD and PAGE are *logical* addresses. */
void
map_page(page_dir *pd, page *pg, u_long addr, int flags)
{
    u_long i = PAGE_DIR_OFFSET(addr);
    page_table *pt;
    DB(("map_page: pd=%#x pg=%#x addr=%#x flags=%#x\n",
	pd, pg, addr, flags));
    if(!(pd[i] & PTE_PRESENT))
    {
	/* Create a new page table. */
	pt = (page_table *)alloc_page();
	memsetl(pt, 0, PAGE_ENTRIES);
	pd[i] = TO_PHYSICAL(pt) | PTE_FREEABLE | flags;
    }
    else
	pt = TO_LOGICAL(PTE_GET_ADDR(pd[i]), page_table *);
    i = PAGE_TABLE_OFFSET(addr);
    if((pt[i] & (PTE_PRESENT | PTE_FREEABLE)) == (PTE_PRESENT | PTE_FREEABLE))
	free_page(TO_LOGICAL(PTE_GET_ADDR(pt[i]), page *));
    pt[i] = TO_PHYSICAL(pg) | flags;
#if 1
    /* Is this necessary? */
    flush_tlb();
#endif
}

/* Set the page-table-entry for the physical address ADDR in the page
   directory PD to PTE. Makes no account of the current contents of this
   pte. */
void
set_pte(page_dir *pd, u_long addr, u_long pte)
{
    u_long i = PAGE_DIR_OFFSET(addr);
    page_table *pt;
    DB(("set_pte: pd=%#x addr=%#x pte=%#x\n",
	pd, addr, pte));
    if(!(pd[i] & PTE_PRESENT))
    {
	/* Create a new page table. */
	pt = (page_table *)alloc_page();
	memsetl(pt, 0, PAGE_ENTRIES);
	pd[i] = (TO_PHYSICAL(pt) | PTE_PRESENT | PTE_FREEABLE
		 | (pte & (PTE_USER | PTE_READ_WRITE)));
    }
    else
	pt = TO_LOGICAL(PTE_GET_ADDR(pd[i]), page_table *);
    i = PAGE_TABLE_OFFSET(addr);
    pt[i] = pte;
#if 1
    /* Is this necessary? */
    flush_tlb();
#endif
}

/* Map the physical address range START to END so that it appears at linear
   address ADDR. FLAGS will be or'ed into the pte. See map_page(). */
void
map_pages(page_dir *pd, u_long start, u_long end, u_long addr, int flags)
{
    while(start < end)
    {
	map_page(pd, TO_LOGICAL(start, page *), addr, flags);
	start += PAGE_SIZE;
	addr += PAGE_SIZE;
    }
}

/* Returns the page-table-entry corresponding to the linear address
   ADDR in the page table PD. If no page-table exists for this address
   zero will be returned (i.e. not present). */
inline u_long
get_pte(page_dir *pd, u_long addr)
{
    if((pd[PAGE_DIR_OFFSET(addr)] & PTE_PRESENT) == 0)
	return 0;
    else
    {
	page_table *pt = TO_LOGICAL(PTE_GET_ADDR(pd[PAGE_DIR_OFFSET(addr)]),
				    page_table *);
	return pt[PAGE_TABLE_OFFSET(addr)];
    }
}

/* Returns the physical address of the page mapped to linear address
   ADDR in the page directory PD. Returns zero if the page is unmapped. */
inline u_long
read_page_mapping(page_dir *pd, u_long addr)
{
    u_long pte = get_pte(pd, addr);
    if(pte & PTE_PRESENT)
	return PTE_GET_ADDR(pte);
    else
	return 0;
}

/* Returns the physical address of the byte at linear address LINEAR-ADDR.
   Returns zero if that page is not present (unmapped). */
inline u_long
lin_to_phys(page_dir *pd, u_long linear_addr)
{
    u_long phys = read_page_mapping(pd, linear_addr);
    if(phys != 0)
	return phys | PAGE_OFFSET(linear_addr);
    else
	return 0;
}

void
put_pd_val(page_dir *pd, int size, u_long val, u_long lin_addr)
{
    u_long phys = lin_to_phys(pd, lin_addr);
    if(phys == 0)
    {
	page *new = alloc_page();
	map_page(pd, new, lin_addr & PAGE_MASK,
		 PTE_READ_WRITE | PTE_USER | PTE_PRESENT);
	phys = TO_PHYSICAL(new) + PAGE_OFFSET(lin_addr);
    }
    switch(size)
    {
    case 1:
	*TO_LOGICAL(phys, u_char *) = val;
	break;
    case 2:
	*TO_LOGICAL(phys, u_short *) = val;
	break;
    case 4:
	*TO_LOGICAL(phys, u_long *) = val;
	break;
    }
}

u_long
get_pd_val(page_dir *pd, int size, u_long lin_addr)
{
    u_long phys = lin_to_phys(pd, lin_addr);
    if(phys != 0)
    {
	switch(size)
	{
	case 1:
	    return *TO_LOGICAL(phys, u_char *);
	case 2:
	    return *TO_LOGICAL(phys, u_short *);
	case 3:
	    return *TO_LOGICAL(phys, u_long *);
	}
    }
    return 0;
}

/* Create and return a new page directory for a kernel task. Basically this is
   just a mapping of the kernel's page tables at PHYS_MAP_ADDR; everything
   else is simply left unmapped. */
page_dir *
make_task_page_dir(void)
{
    page_dir *pd = (page_dir *)alloc_page();
    if(pd != NULL)
    {
	memsetl(pd, 0, PAGE_ENTRIES - NUM_KERNEL_PAGE_TABLES);
	memcpy(pd + PAGE_DIR_OFFSET(KERNEL_BASE_ADDR),
	       logical_kernel_pd + PAGE_DIR_OFFSET(KERNEL_BASE_ADDR),
	       NUM_KERNEL_PAGE_TABLES * 4);
    }
    return pd;
}

/* If the linear addresses from START to START+EXTENT are all present (i.e.
   a page is mapped to that address) return TRUE. If any of the page frames
   in this range are unmapped return FALSE. */
bool
check_area(page_dir *pd, u_long start, size_t extent)
{
    u_long addr = start & PAGE_MASK;
    extent += start - addr;
    while(1)
    {
	if((get_pte(pd, addr) & PTE_PRESENT) == 0)
	    return FALSE;
	if(extent <= PAGE_SIZE)
	    return TRUE;
	extent -= PAGE_SIZE;
	addr += PAGE_SIZE;
    }
    return TRUE;
}



void
init_mm(void)
{
    logical_kernel_pd = TO_LOGICAL(kernel_page_dir, page_dir *);
    /* Set kernel_brk. */
    kernel_brk = (u_char *)logical_top_of_kernel;
}

void
describe_mm(struct shell *sh)
{
    sh->shell->printf(sh, "Total: %d  Used: %d  Free: %d\n",
		      (available_pages + used_pages) * 4,
		      used_pages * 4, available_pages * 4);
    sh->shell->printf(sh, "Kernel break: %#x\n", kernel_sbrk(0));
}
