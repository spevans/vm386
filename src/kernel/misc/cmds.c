/* cmds.c -- Shell commands to do kernel stuff.
   John Harper. */

#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/irq.h>
#include <vmm/dma.h>
#include <vmm/cookie_jar.h>
#include <vmm/time.h>
#include <vmm/page.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/traps.h>
#include <vmm/fs.h>
#include <vmm/types.h>

extern void example_task(void);


/* This code handles registering and de-registering shell commands. The
   benefit of this code is that it works even if the shell hasn't been
   loaded (or at least it will work when the shell eventually gets loaded). */

static struct shell_cmds *pending_cmds;

void
add_shell_cmds(struct shell_cmds *cmds)
{
    struct shell_module *shell;
    forbid();
    shell = (struct shell_module *)find_module("shell");
    if(shell != NULL)
	shell->add_cmd_list(cmds);
    else
    {
	cmds->next = pending_cmds;
	pending_cmds = cmds;
    }
    permit();
}

void
remove_shell_cmds(struct shell_cmds *cmds)
{
    struct shell_module *shell;
    forbid();
    shell = (struct shell_module *)find_module("shell");
    if(shell != NULL)
	shell->remove_cmd_list(cmds);
    else
    {
	struct shell_cmds **x;
	x = &pending_cmds;
	while(*x != NULL)
	{
	    if(*x == cmds)
	    {
		*x = (*x)->next;
		break;
	    }
	    x = &(*x)->next;
	}
    }
    permit();
}

void
collect_shell_cmds(void)
{
    struct shell_module *shell;
    struct shell_cmds *x;
    forbid();
    shell = (struct shell_module *)find_module("shell");
    x = pending_cmds;
    if(shell != NULL)
    {
	pending_cmds = NULL;
	while(x != NULL)
	{
	    shell->add_cmd_list(x);
	    x = x->next;
	}
    }
    permit();
}


/* Kernel shell commands. */

#define DOC_sysinfo "sysinfo OPTIONS...\n\
Prints text describing the current state of the system. Options\n\
available are:\n\n\
	`-irq'		IRQ handler details.\n\
	`-dma'		DMA channel allocations.\n\
	`-mods'		Details of loaded modules.\n\
	`-mm'		Current memory management state.\n\
	`-ps'		Print current tasks."
int
cmd_sysinfo(struct shell *sh, int argc, char **argv)
{
    while(argc > 0)
    {
	if(!strcmp("-irq", *argv))
	    describe_irqs(sh);
	else if(!strcmp("-dma", *argv))
	    describe_dma(sh);
	else if(!strcmp("-mods", *argv))
	    describe_modules(sh);
	else if(!strcmp("-mm", *argv))
	    describe_mm(sh);
	else if(!strcmp("-ps", *argv))
	    describe_tasks(sh);
	else
	    sh->shell->printf(sh, "Error: unknown option `%s'\n", *argv);
	argc--; argv++;
    }
    return 0;
}

#define DOC_cookie "cookie\n\
Show the contents of the cookie jar."
int
cmd_cookie(struct shell *sh, int argc, char **argv)
{
    static char np[] = "Not Present";
    static char *Floppy_types[] = {
        np, "360K 5.25\"", "1.2M 5.25\"", "720K 3.5\"", "1.44M 3.5\""
    };
    int i;

    sh->shell->printf(sh, "Total Memory %dK\n", cookie.total_mem); 
    sh->shell->printf(sh, "CPU: 80%d86\tFPU: ", cookie.proc.cpu_type);
    switch(cookie.proc.fpu_type) {
	case 0:
	sh->shell->printf(sh, "%s\n", np);
	break;

	case 1:
	sh->shell->printf(sh, "present\n");
	break;

	case 2:
	sh->shell->printf(sh, "80287\n");
	break;

	case 3:
	sh->shell->printf(sh, "80387\n");
	break;
    }
    for(i = 0; i < 4; i++) {
        sh->shell->printf(sh, "COM%d: ", i+1);
	if(cookie.comports[i]) 
    	    sh->shell->printf(sh, "base: %Xh\t", cookie.comports[i]);
	else
	    sh->shell->printf(sh, "%s\t", np);	
	if(i & 1)
		sh->shell->printf(sh, "\n");
    }
    for(i = 0; i < 4; i++) {
        sh->shell->printf(sh, "LPR%d: ", i+1);
	if(cookie.lprports[i]) 
    	    sh->shell->printf(sh, "base: %Xh\t", cookie.lprports[i]);
	else
	    sh->shell->printf(sh, "%s\t", np);	
	if(i & 1)
		sh->shell->printf(sh, "\n");
    }
    for(i = 0; i < 2; i++) 
        sh->shell->printf(sh, "Floppy Drive %d is %s\n", i+1,
            Floppy_types[cookie.floppy_types[i]]);
    sh->shell->printf(sh, "%d Fixed disks found\n", cookie.total_hdisks);
    for(i = 0; i < cookie.total_hdisks; i++) {
        sh->shell->printf(sh, "Fixed disk %d:\n", i+1);
	sh->shell->printf(sh, "Heads: %d\tCylinders: %d\tSectors: %d\n",
			  cookie.hdisk[i].heads,
			  cookie.hdisk[i].cylinders,
			  cookie.hdisk[i].sectors);
        sh->shell->printf(sh, "Precomp: %d\tlanding zone: %d\n",
			  cookie.hdisk[i].precomp,
			  cookie.hdisk[i].lz);
    }
    sh->shell->printf(sh, "\n");
    return 0;
}

#define DOC_date "date\n\
Display the current date."
int
cmd_date(struct shell *sh, int argc, char **argv)
{
    time_t time = current_time();
    struct time_bits tm;
    expand_time(time, &tm);
    sh->shell->printf(sh, "%3s %-2d %3s %2d:%02d:%02d %4d\n",
		      tm.dow_abbrev, tm.day, tm.month_abbrev, tm.hour,
		      tm.minute, tm.second, tm.year);
    return 0;
}


#define DOC_task "task\n\
Add a new test task."
int
cmd_task(struct shell *sh, int argc, char **argv)
{
    struct task *task;

    task = add_task(example_task, TASK_RUNNING, -1, "test");
    if(task == NULL) {
       sh->shell->printf(sh, "Task Creation Failure\n");
       return 1;
    } else {
      sh->shell->printf(sh, "Task Created: pid = %d\n", task->pid);
    }
    return 0;
}


#define DOC_kill "kill PID\n\
Kill task or virtual machine with pid PID."
int
cmd_kill(struct shell *sh, int argc, char *argv[])
{
    u_long pid;
    struct task *task;
    if(argc < 1) {
        sh->shell->printf(sh, "pid not specified\n");
        return 1;
     }
     forbid();
     pid = strtoul(argv[0], NULL, 10);
     task = find_task_by_pid(pid);
     if(task == NULL)
     {
	 permit();
	 sh->shell->printf(sh, "No task with pid %d\n", pid);
	 return 1;
     }
     if(task->flags & TASK_VM)
     {
	 struct vm_module *vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
	 if(vm != NULL)
	 {
	     vm->kill_vm(task->user_data);
	     kernel->close_module((struct module *)vm);
	 }
     }
     else if(kill_task(task))
     {
         sh->shell->printf(sh, "Cant kill task %d\n", (u_int32)pid);
	 permit();
         return 1;
     }
     sh->shell->printf(sh, "Task %d killed\n", (u_int32)pid);
     permit();
     return 0;
}

#define DOC_freeze "freeze PIF\n\
Stop the task with pid PID from executing."
int
cmd_freeze(struct shell *sh, int argc, char **argv)
{
    u_long pid;
    u_long flags;
    int rc = RC_FAIL;
    struct task *task;
    if(argc == 1)
    {
	save_flags(flags);
	cli();
	pid = strtoul(argv[0], NULL, 10);
	task = find_task_by_pid(pid);
	if(task != NULL)
	{
	    task->flags |= TASK_FROZEN;
	    suspend_task(task);
	    rc = 0;
	    load_flags(flags);
	}
	else
	{
	    load_flags(flags);
	    sh->shell->printf(sh, "No task with pid %d\n", pid);
	}
    }
    return rc;
}

#define DOC_thaw "thaw PID\n\
Unfreeze a task stopped by the shell command `freeze'."
int
cmd_thaw(struct shell *sh, int argc, char **argv)
{
    u_long pid;
    u_long flags;
    int rc = RC_FAIL;
    struct task *task;
    if(argc == 1)
    {
	save_flags(flags);
	cli();
	pid = strtoul(argv[0], NULL, 10);
	task = find_task_by_pid(pid);
	if((task != NULL) && (task->flags & TASK_FROZEN))
	{
	    task->flags &= ~TASK_FROZEN;
	    wake_task(task);
	    rc = 0;
	    load_flags(flags);
	}
	else
	{
	    load_flags(flags);
	    sh->shell->printf(sh, "No task with pid %d\n", pid);
	}
    }
    return rc;
}

#define DOC_open "open MODULE-NAME\n\
Open the module MODULE, ensuring that it's loaded into memory."
int
cmd_open(struct shell *sh, int argc, char **argv)
{
    struct module *mod;
    if(argc < 1)
	return 0;
    mod = open_module(*argv, SYS_VER);
    if(mod == NULL)
    {
	sh->shell->printf(sh, "Can't open module `%s'\n", *argv);
	return RC_FAIL;
    }
    else
    {
	close_module(mod);
	return 0;
    }
}

#define DOC_expunge "expunge MODULE-NAME\n\
Attempt to flush the module MODULE-NAME out of memory."
int
cmd_expunge(struct shell *sh, int argc, char **argv)
{
    if(argc < 1)
	return 0;
    if(expunge_module(*argv))
	return 0;
    sh->shell->printf(sh, "Can't expunge module `%s'\n", *argv);
    return RC_WARN;
}

#define DOC_sleep "sleep SECONDS\n\
Put the shell to sleep for SECONDS seconds."
int
cmd_sleep(struct shell *sh, int argc, char **argv)
{
    time_t secs;
    if(argc < 1)
	return 0;
    secs = strtoul(argv[0], NULL, 0);
    sleep_for(secs);
    return 0;
}

static struct shell_cmds kernel_cmds =
{
    0,
    { CMD(sysinfo), CMD(cookie), CMD(date), CMD(task), CMD(kill),
      CMD(freeze), CMD(thaw), CMD(open), CMD(expunge), CMD(sleep),
      END_CMD }
};

bool
add_kernel_cmds(void)
{
    add_shell_cmds(&kernel_cmds);
    return TRUE;
}
