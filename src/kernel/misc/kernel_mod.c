/* kernel_mod.c -- Pseudo-module of the kernel.
   John Harper. */

#include <vmm/kernel.h>
#include <vmm/fs.h>
#include <vmm/shell.h>
#include <vmm/hd.h>
#include <vmm/floppy.h>
#include <vmm/irq.h>
#include <vmm/dma.h>
#include <vmm/cookie_jar.h>
#include <vmm/debug.h>
#include <vmm/tasks.h>
#include <vmm/traps.h>
#include <vmm/vm.h>
#include <vmm/errno.h>

extern char root_dev[];

struct kernel_module kernel_module =
{
    MODULE_INIT("kernel", SYS_VER, NULL, NULL, NULL, NULL),

    /* module functions */
    find_module, open_module, close_module, expunge_module,

    /* mm functions */
    alloc_page, alloc_pages_64, free_page, free_pages, map_page, set_pte,
    get_pte, read_page_mapping, lin_to_phys, put_pd_val, get_pd_val,
    check_area,

    /* kernel malloc */
    malloc, calloc, free, realloc, valloc,

    /* task functions */
    schedule, wake_task, suspend_task, suspend_current_task,
    add_task, kill_task, find_task_by_pid,
    add_task_list, remove_task_list, sleep_in_task_list,
    wake_up_task_list, wake_up_first_task,

    /* IRQ functions */
    alloc_irq, dealloc_irq, alloc_queued_irq, dealloc_queued_irq,

    /* DMA functions */
    alloc_dmachan, dealloc_dmachan, setup_dma,

    /* misc functions */
    kvsprintf, ksprintf, kvprintf, kprintf, set_kprint_func,
    get_gdt, set_debug_reg, error_string, dump_regs,
    strtoul, strdup, get_fs_module,

    /* Time functions. */
    add_timer, remove_timer, sleep_for_ticks, sleep_for,
    expand_time, current_time, get_timer_ticks, udelay, 

    /* shell commands. */
    add_shell_cmds, remove_shell_cmds, collect_shell_cmds,

    /* system variables passed by the startup code */

    &cookie, root_dev, NULL, FALSE
};

/* All modules statically linked into the kernel (by ld) use this pointer
   to reference the kernel module. */
struct kernel_module *kernel;

static struct fs_module *fs;

extern void *_text_start, *_rodata_start, *_data_start, *_bss_start;
static struct module_memory modmem;
void
kernel_mod_init(void)
{
    /* Initialise this explicitly so that the actual variable is common (and
       therefore is shared between all modules statically linked into the
       kernel). */
    kernel = &kernel_module;
    kernel_module.base.is_static = TRUE;
    modmem.text = _text_start;
    modmem.rodata = _rodata_start;
    modmem.data = _data_start;
    modmem.bss = _bss_start;
    kernel_module.base.mod_memory = &modmem;
    kernel_module.base.mod_size = ((u_long)logical_top_of_kernel
				   - (u_long)_text_start);
    add_module(&kernel_module.base);
    kernel_module.base.open_count = 1;
    add_kernel_cmds();
}

void
kernel_mod_init2(void)
{
    struct module *shell;

    fs = (struct fs_module *)open_module("fs", SYS_VER);

    /* Open the shell; this should be enough for it to get a toe-hold
       in the system :) */
    shell = open_module("shell", SYS_VER);
    if(shell == NULL)
    {
	kprintf("Can't open shell.module; stopping\n");
	/* No point going any further. */
	while(1) ;
    }
#if 0
    /* Don't ever close the shell. */
    close_module(shell);
#endif
    collect_shell_cmds();
}


/* Allow other parts of the kernel module and other modules to access the `fs'
   module which has to be statically linked in anyway */
struct fs_module *
get_fs_module(void)
{
    return fs;
}
