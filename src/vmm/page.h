/* page.h -- definitions for page management.
   John Harper. */

#ifndef _VMM_PAGE_H
#define _VMM_PAGE_H

#include <vmm/types.h>
#include <vmm/map.h>

#define PAGE_SIZE 4096
#define PAGE_ENTRIES (PAGE_SIZE / 4)
#define PAGE_MASK 0xfffff000
#define PAGE_OFFSET_MASK 0x00000fff
#define PAGE_BITS 12

#define PAGE_TABLE_BYTES (1024 * PAGE_SIZE)
#define PAGE_DIR_BYTES   (1024 * PAGE_TABLE_BYTES)

typedef union _page {
    union _page *next_free;
    char mem[PAGE_SIZE];
} page;

/* Page-table-entry layout.
    31              12  11    8               0
   +------------------+------------------------+
   | Page frame       |                   U R  |
   |   Address        | AVAIL 0 0 D A 0 0 / / P|
   |     31..12       |                   S W  |
   +------------------+------------------------+

   Note that the format of a page-directory-entry is exactly the same. */

#define PTE_ADDR	0xfffff000
#define PTE_AVAIL	0x00000e00
#define PTE_DIRTY	0x00000040
#define PTE_ACCESSED	0x00000020
#define PTE_USER	0x00000004
#define PTE_READ_WRITE	0x00000002
#define PTE_PRESENT	0x00000001

/* System-defined bits in the PTE_AVAIL field. */
#define PTE_FREEABLE	0x00000200	/* Page may be freed. */

#define PTE_GET_ADDR(x)	((x) & PTE_ADDR)

/* These are actually 4096-byte sized arrays of u_long's, but they're
   nicer to handle in C this way. */
typedef u_long page_table;
typedef u_long page_dir;

/* Find the byte offset (i.e. 0->4095) of the address X in its page. */
#define PAGE_OFFSET(x)		((u_long)(x) & PAGE_OFFSET_MASK)

/* Find the u_long offset (i.e. 0->1023) of the endtry for the address X
   in its page table. */
#define PAGE_TABLE_OFFSET(x)	((((u_long)(x)) / PAGE_SIZE) & 1023)

/* Find the u_long offset (i.e. 0->1023) of the entry for the address X
   in its page directory. */
#define PAGE_DIR_OFFSET(x)	(((u_long)(x)) / (PAGE_SIZE * 1024))

/* Bit definitions for the error code pushed by the page fault exception. */
#define PF_ERROR_PROTECTION	1
#define PF_ERROR_WRITE		2
#define PF_ERROR_USER		4

static inline void
flush_tlb(void)
{
    asm volatile ("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3" : : : "ax");
}

static inline u_long
get_cr2(void)
{
    u_long res;
    asm ("movl %%cr2,%0" : "=r" (res));
    return res;
}

#define SWAP_CR3(task, new_cr3, old_cr3)	\
do {						\
    u_long flags;				\
    save_flags(flags);				\
    cli();					\
    asm ("movl %%cr3,%0; movl %1,%%cr3"		\
	 : "=r" (old_cr3)			\
	 : "r" (new_cr3));			\
    task->tss.cr3 = new_cr3;			\
    load_flags(flags);				\
} while(0)

#ifdef KERNEL

/* forward refs. */
struct trap_regs;
struct shell;

/* from page.c */
extern page_dir *logical_kernel_pd;
extern page *alloc_page(void);
extern page *alloc_pages_64(u_long n);
extern void free_page(page *page);
extern void free_pages(page *page, u_long n);
extern void add_pages(u_long start, u_long end);
extern u_long free_page_count(void);
extern void delete_page_dir(page_dir *pd);
extern void delete_page_table(page_table *pt);
extern void map_page(page_dir *pd, page *page, u_long addr, int flags);
extern void set_pte(page_dir *pd, u_long addr, u_long pte);
extern void map_pages(page_dir *pd, u_long start, u_long end, u_long addr, int flags);
extern u_long get_pte(page_dir *pd, u_long addr);
extern u_long read_page_mapping(page_dir *pd, u_long addr);
extern u_long lin_to_phys(page_dir *pd, u_long linear_addr);
extern void put_pd_val(page_dir *pd, int size, u_long val, u_long lin_addr);
extern u_long get_pd_val(page_dir *pd, int size, u_long lin_addr);
extern page_dir *make_task_page_dir(void);
extern bool check_area(page_dir *pd, u_long start, size_t extent);
extern void init_mm(void);
extern void describe_mm(struct shell *sh);

/* from fault.c */
extern void page_exception_handler(struct trap_regs *regs);

/* from sbrk.c */
extern u_char *kernel_brk;
extern void *kernel_sbrk(long);

#endif /* KERNEL */
#endif /* _VMM_PAGE_H */
