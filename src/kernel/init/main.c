#include <vmm/io.h>
#include <vmm/page.h>
#include <vmm/traps.h>
#include <vmm/segment.h>
#include <vmm/module.h>
#include <vmm/kernel.h>
#include <vmm/tty.h>
#include <vmm/irq.h>
#include <vmm/string.h>
#include <vmm/page.h>
#include <vmm/cookie_jar.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/time.h>

extern char end, edata, etext;
extern char root_dev[];

unsigned int *PhysicalMem = (unsigned int *)PHYS_MAP_ADDR;

void main_kernel(void);

/* This is run as a task; it finishes the initialisation procedure. The
   reason for this is that the modules probably assume that they're able
   to suspend the current process, for this an idle task is needed.. */
static void
init_task(void)
{
    kernel_mod_init();
    init_static_modules();
    kernel_mod_init2();
    syslogd = (struct syslogd_module *)open_module("syslogd", SYS_VER);

    /* flush the printk ring buffer to the syslog daemon */
    if(syslogd != NULL) {
        printk("syslogd active\n");
    }
}

void main_kernel()
{
	unsigned int total_mem = (unsigned int)cookie.total_mem;

        update_cmos_date();
	total_mem *= 1024;
	memsetw(TO_LOGICAL(0xb8000, void *), 0x0720, 80*25);

	add_pages((u_long)logical_top_of_kernel, 0xA0000);
	add_pages(0x100000, total_mem);

	printk("End of Static Kernel at %08X\n", &end);
	printk("End of Kernel data at %08X\n", &edata);
	printk("End of Kernel text at %08X\n", &etext);
	printk("Page Directory at %08X\n",kernel_page_dir);
	printk("Total Memory = %dK\n", total_mem / 1024);
	printk("%d Pages added giving %dK free\n", free_page_count(),
		free_page_count() * 4);

	printk("Rootdev: %s\n", &root_dev);
	printk("Initialising memory management..");
	init_mm();
	printk(" done.\n");
	printk("Initialising trap handlers..");
	init_trap_gates();
	printk(" done.\n");
	printk("Initialising irq handlers..");
	init_irq_handlers();
	printk(" done.\n");
	printk("Initialising scheduler..");
	init_sched();
	printk(" done.\n");
	printk("Initialising kernel task.. ");
	printk("pid = %d.. done\n", add_initial_task());
	sti();
	printk("Initialising timer..");
	init_time();
	printk(" done.\n");
	printk("Initialising irq-dispatcher...");
	init_queued_irqs();
	printk(" done.\n");

	add_task(init_task, TASK_RUNNING, 0, "init");

	for(;;)
	{
	    /* Don't need to call service_queued_irqs() anymore,
	       that's taken care of by the irq stubs, so we may
	       as well just stop 'til the next irq. */
#if 1
	    hlt();
#else
	    char *scr = TO_LOGICAL(0xB8000 + 1680, u_char *);
	    *(scr+1) = 7;
	    while(1)
		(*scr)++;
#endif
	}
}
