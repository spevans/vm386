/* kernel.h -- Definition of the kernel module.
   John Harper. */

#ifndef _VMM_KERNEL_H
#define _VMM_KERNEL_H

#include <vmm/module.h>
#include <vmm/page.h>
#include <vmm/segment.h>
#include <vmm/lists.h>
#include <stdarg.h>

/* Forward declarations. */
struct task;
struct task_list;
struct timer_req;
struct time_bits;
struct DmaBuf;
struct trap_regs;
struct shell;
struct shell_cmds;

/* This module is the `glue' which holds all the modules together. Each
   module gets passed a pointer to this module when it's initialised
   so that it can call the standard kernel functions (and therefore open
   other modules). */
struct kernel_module
{
    struct module base;

    /* Module handling functions. */
    struct module *(*find_module)(const char *name);
    struct module *(*open_module)(const char *name, u_short version);
    void (*close_module)(struct module *mod);
    bool (*expunge_module)(const char *name);

    /* Memory management functions. */
    page *(*alloc_page)(void);
    page *(*alloc_pages_64)(u_long n);
    void (*free_page)(page *page);
    void (*free_pages)(page *page, u_long n);
    void (*map_page)(page_dir *pd, page *p, u_long addr, int flags);
    void (*set_pte)(page_dir *pd, u_long addr, u_long pte);
    u_long (*get_pte)(page_dir *pd, u_long addr);
    u_long (*read_page_mapping)(page_dir *pd, u_long addr);
    u_long (*lin_to_phys)(page_dir *pd, u_long lin_addr);
    void (*put_pd_val)(page_dir *pd, int size, u_long val, u_long lin_addr);
    u_long (*get_pd_val)(page_dir *pd, int size, u_long lin_addr);
    bool (*check_area)(page_dir *pd, u_long start, size_t extent);

    /* Kernel malloc functions. */
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nelem, size_t size);
    void (*free)(void *ptr);
    void *(*realloc)(void *ptr, size_t size);
    void *(*valloc)(size_t size);

    /* Task handling functions. */
    void (*schedule)(void);
    void (*wake_task)(struct task *task);
    void (*suspend_task)(struct task *task);
    void (*suspend_current_task)(void);
    struct task *(*add_task)(void *task, u_long flags, short pri,
			     const char *name);
    int (*kill_task)(struct task *task);
    struct task *(*find_task_by_pid)(u_long pid);

    void (*add_task_list)(struct task_list **head, struct task_list *elt);
    void (*remove_task_list)(struct task_list **head, struct task_list *elt);
    void (*sleep_in_task_list)(struct task_list **head);
    void (*wake_up_task_list)(struct task_list **head);
    void (*wake_up_first_task)(struct task_list **head);
    
    /* IRQ functions. */
    bool (*alloc_irq)(u_int irq, void *func, char *name);
    void (*dealloc_irq)(u_int irq);
    bool (*alloc_queued_irq)(u_int irq, void (*func)(void), char *name);
    void (*dealloc_queued_irq)(u_int irq);

    /* DMA functions. */
    bool (*alloc_dmachan)(u_int chan, char *name);
    void (*dealloc_dmachan)(u_int chan);
    void (*setup_dma)(struct DmaBuf *, u_int8);

    /* Misc functions. */
    void (*vsprintf)(char *buf, const char *fmt, va_list args);
    void (*sprintf)(char *buf, const char *fmt, ...);
    void (*vprintf)(const char *fmt, va_list args);
    void (*printf)(const char *fmt, ...);
    void (*set_print_func)(void (*func)(const char *, size_t));
    desc_table *(*get_gdt)(void);
    void (*set_debug_reg)(int reg_set, u_long addr, int type, int size);
    const char *(*error_string)(int errno);
    void (*dump_regs)(struct trap_regs *regs, bool stop);
    u_long (*strtoul)(const char *str, char **ptr, int base);
    char * (*strdup)(const char *str);

    /* Time handling. */
    void (*add_timer)(struct timer_req *req);
    void (*remove_timer)(struct timer_req *req);
    void (*sleep_for_ticks)(u_long ticks);
    void (*sleep_for)(time32_t length);
    void (*expand_time)(time32_t cal, struct time_bits *tm);
    time32_t (*current_time)(void);
    u_long (*get_timer_ticks)(void);
    void (*udelay)(u_long usecs);

    /* Shell command handling */
    void (*add_shell_cmds)(struct shell_cmds *cmds);
    void (*remove_shell_cmds)(struct shell_cmds *cmds);
    void (*collect_shell_cmds)(void);

    /* Cached system info. */
    struct cookie_jar *cookie;
    char *root_dev;

    /* A pointer to the task currently executing. */
    struct task *current_task;
    bool need_resched;

    /* Note that I've got rid of the module pointers for a reason. Each
       user of a module should usually open it themselves.. */
};    


/* Some function prototypes. */

extern struct kernel_module *kernel; /* Defined separately in *each* module */

#ifdef KERNEL

/* from kernel_mod.c */
extern struct kernel_module kernel_module;
extern struct fs_module *fs;
extern void kernel_mod_init(void);
extern void kernel_mod_init2(void);

/* from printf.c */
extern void kvsprintf(char *buf, const char *fmt, va_list args);
extern void ksprintf(char *buf, const char *fmt, ...);
extern void kvprintf(const char *fmt, va_list args);
extern void kprintf(const char *fmt, ...);
extern void set_kprint_func(void (*func)(const char *, size_t));

/* from cmds.c */
extern void add_shell_cmds(struct shell_cmds *cmds);
extern void remove_shell_cmds(struct shell_cmds *cmds);
extern void collect_shell_cmds(void);

extern bool add_kernel_cmds(void);

#define printk kprintf

/* misc.. */
extern void set_intr_gate(int, void *);
extern void set_trap_gate(int, void *);
extern u_long kernel_page_dir;
extern unsigned int *PhysicalMem;
extern u_long logical_top_of_kernel;
extern u_char bios_drive_info[32];
extern desc_table *get_gdt(void);
extern struct syslogd_module *syslogd;
extern char *dump_syslog(void);
extern void add_to_syslog(char *entry, int slen);

/* from alloc.o */
extern void *malloc(size_t size);
extern void *calloc(size_t nelem, size_t size);
extern void free(void *ptr);
extern void *realloc(void *ptr, size_t size);
extern void *valloc(size_t size);

/* from lib.c */
extern u_long strtoul(const char *str, char **ptr, int base);
extern char *strdup(const char *str);

#endif /* KERNEL */
#endif /* _VMM_KERNEL_H */
