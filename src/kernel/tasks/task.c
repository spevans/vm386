/* Task Handling Routines */

#include <vmm/tasks.h>
#include <vmm/page.h>
#include <vmm/kernel.h>
#include <vmm/segment.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <vmm/fs.h>
#include <vmm/shell.h>
#include <vmm/irq.h>
#include <vmm/time.h>

static u_int32	next_pid = 1;
static int TaskCount = 0;
static struct task TaskArray[MAX_TASKS];

/* Find the next free TSS and fill it in */
int add_tss_gdt(struct tss *tss)
{
  #define TSS_ATTR	(0x1089 << 8)	/* Avl = 1, Present,  Limit 19..16 = 0
				           type = available 386 TSS */
  int i;
  unsigned long addr = (unsigned int)tss;
  u_long flags;

  save_flags(flags);
  cli();

  for(i = FIRST_TSS; i < (FIRST_TSS + MAX_TASKS); i++) {
      /* Available to Software bit is used as a free TSS marker. If
         0, TSS is free else it is used */
      if((GDT[i].hi & 0x00100000) == 0) break;	
  }
  /* i is now a free TSS */

  addr += 0xF8000000;	/* logical to linear */

  GDT[i].lo = ((addr & 0xffff) << 16) | (sizeof(struct tss) -1);
  GDT[i].hi = (addr & 0xff000000) |
                     ((addr & 0x00ff0000) >> 16) |
                     TSS_ATTR;

  load_flags(flags);

  return i*8;
}


static struct task *
alloc_task(void)
{
    int i;
    u_long flags;
    save_flags(flags);
    cli();
    for(i = 0; i < MAX_TASKS; i++)
    {
        if(TaskArray[i].pid == 0) {
	    memset(&TaskArray[i], 0, sizeof(struct task));
	    load_flags(flags);
	    return &TaskArray[i];
	}
    }
    load_flags(flags);
    return NULL;
}
 
static inline void
free_task(struct task *task)
{
    task->pid = 0;
}

int
add_initial_task()
{
	struct task *task;

	if(TaskCount == MAX_TASKS) return -1;

	task = alloc_task();
	task->stack0 = alloc_page();
	task->stack = NULL;
	if(task->stack0 == NULL) return -1;
	task->tss.ss0 = KERNEL_DATA;
	task->tss.esp0 = (unsigned long)(task->stack0 + 4092);
	task->tss.back_link = 0;
	task->tss.trace = 0;
	task->tss.bitmap = sizeof(struct tss);
	task->page_dir = logical_kernel_pd;
	task->tss.cr3 = kernel_page_dir;
	task->pid = next_pid;
	task->ppid = 0;
	task->tss_sel = add_tss_gdt(&task->tss);
	task->flags = TASK_RUNNING | TASK_IMMORTAL;
	task->pri = -80;
	task->name = "idle";
	task->last_sched = timer_ticks;
	task->quantum = STD_QUANTUM;

	current_task = task;
	kernel_module.current_task = task;
	append_task(task);
	ltr(task->tss_sel);
	TaskCount++;
	return next_pid++;
}


static int
create_tss(struct task *task, void *code)
{
    task->stack0 = alloc_page();
    if(task->stack0 != NULL)
    {
	task->stack = alloc_page();
	if(task->stack != NULL)
	{
	    task->tss.esp0 = (u_long)task->stack0 + 4092;
	    task->tss.ss0 = 0x20;
	    task->tss.eip = (unsigned long)code;
	    task->tss.eax = 0xAAAAAAAA;
	    task->tss.ebx = 0xBBBBBBBB;
	    task->tss.ecx = 0xCCCCCCCC;
	    task->tss.edx = 0xDDDDDDDD;
	    task->tss.esi = 0x51515151;
	    task->tss.edi = 0x01010101;
	    task->tss.ebp = 0x86868686;
	    task->tss.eflags = 0x200;
	    task->tss.cs = KERNEL_CODE;
	    task->tss.ds = KERNEL_DATA; 
	    task->tss.es = KERNEL_DATA; 
	    task->tss.fs = USER_DATA; 
	    task->tss.gs = KERNEL_DATA; 
	    task->tss.trace = 0;
	    task->tss.bitmap = sizeof(struct tss);	/* Null I/O bitmap */
	    task->tss.esp = (u_long)task->stack + 4092;
	    task->tss.ss = KERNEL_DATA;
	    task->tss.cr3 = TO_PHYSICAL(task->page_dir);
	    task->tss.back_link = 0;
	    return 0;
	}
	free_page(task->stack0);
    }
    return 1;
}

struct task *
add_task(void *task, u_long flags, short pri, const char *name)
{
    struct task *new_task;
    if(TaskCount != MAX_TASKS)
    {
	new_task = alloc_task();
	if(new_task != NULL)
	{
	    new_task->page_dir = make_task_page_dir();
	    if(new_task->page_dir != NULL)
	    {
		if(!create_tss(new_task, task))
		{
		    new_task->tss_sel = add_tss_gdt(&new_task->tss);
		    new_task->pid = next_pid++;
		    new_task->ppid = current_task->pid;
		    new_task->flags = flags;
		    new_task->pri = pri;
		    new_task->name = name;
		    new_task->quantum = STD_QUANTUM;
		    new_task->errno = 0;
		    if(current_task->current_dir)
		    {
			new_task->current_dir = fs->dup(current_task->current_dir);
		    }
		    else
			new_task->current_dir = NULL;
		    /* Push the address of a routine to kill the current
		       task onto the new task's stack, so that if it returns,
		       it dies.. */
		    u_long *tmpesp = (u_long *)new_task->tss.esp;
		    tmpesp--;
		    new_task->tss.esp = (u_long)tmpesp;
		    *tmpesp = (u_long)kill_current_task;
		    append_task(new_task);	
		    TaskCount++;
		    return new_task;
		}
		delete_page_dir(new_task->page_dir);
	    }
	    free_task(new_task);
	}
    }
    return NULL;
}


void
reclaim_task(struct task *task)
{
    int i;
    u_long flags;
    save_flags(flags);
    cli();
    /* Now remove its TSS entry. All we have to do is clear the AVL bit
       but we will clear the whole tss to capture any errors that may
       occur if it is referenced accidently */
    i = task->tss_sel / 8;
    GDT[i].lo = 0; 
    GDT[i].hi = 0; 
    if(task->stack0 != NULL)
	free_page(task->stack0);
    if(task->stack != NULL)
	free_page(task->stack);
    if(task->page_dir != NULL)
	delete_page_dir(task->page_dir);
    free_task(task);
    TaskCount--;
    load_flags(flags);
}

/* Kill TASK. This *may not* be called by interrupt handlers. */
int
kill_task(struct task *task)
{
    u_long flags;

    if(task->flags & TASK_IMMORTAL)
	return -1;
    if(task->current_dir != NULL)
	fs->close(task->current_dir);

    save_flags(flags); 
    cli();
    remove_task(task);
    if(current_task != task)
    {
	/* Okay to reclaim the task immediately since it's not running. */
	reclaim_task(task);
    }
    else
    {
	/* We're killing the current task; schedule() will arrange for
	   reclaim_task() to be called when safe. */
	schedule();
    }
    /* return ok */
    load_flags(flags);
    return 0;
}

void
kill_current_task(void)
{
    /* People are always allowed to kill themselves :) */
    current_task->flags &= ~TASK_IMMORTAL;
    kill_task(current_task);
}

struct task *
find_task_by_pid(u_long pid)
{
    u_int i;
    u_long flags;
    if(pid == 0)
	return NULL;
    save_flags(flags);
    cli();
    for(i = 0; i < MAX_TASKS; i++)
    {
	if(TaskArray[i].pid == pid)
	{
	    load_flags(flags);
	    return &TaskArray[i];
	}
    }
    load_flags(flags);
    return NULL;
}

void
describe_tasks(struct shell *sh)
{
    int i;
    cli();
    sh->shell->printf(sh, "%-16s %5s %5s %4s %8s %4s %8s %8s %3s\n",
		      "Name", "Pid", "PPid", "Tss", "Flags", "Pri",
		      "Sched.", "CPU", "FC");
    for(i = 0; i < MAX_TASKS; i++)
    {
	if(TaskArray[i].pid != 0)
	{
	    sh->shell->printf(sh,
			      "%-16s %5u %5u %04X %08X % 04d %8u %8u % 03d\n",
			      TaskArray[i].name ? : "",
			      TaskArray[i].pid,
			      TaskArray[i].ppid,
			      TaskArray[i].tss_sel,
			      TaskArray[i].flags,
			      TaskArray[i].pri,
			      TaskArray[i].sched_count,
			      TaskArray[i].cpu_time,
			      TaskArray[i].forbid_count);
	}
    }
    sti();
}
