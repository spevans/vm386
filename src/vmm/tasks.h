#ifndef __TASKS_H__
#define __TASKS_H__

#ifndef	__ASM__

#include <vmm/types.h>
#include <vmm/page.h>
#include <vmm/lists.h>
#include <vmm/io.h>
#ifndef KERNEL
# include <vmm/kernel.h>
#endif

/* the first gdt entry for TSS's */
#define FIRST_TSS	5

/* I copied this from Linux, hope it's ok.. */
struct tss {
	unsigned short	back_link,__blh;
	unsigned long	esp0;
	unsigned short	ss0,__ss0h;
	unsigned long	esp1;
	unsigned short	ss1,__ss1h;
	unsigned long	esp2;
	unsigned short	ss2,__ss2h;
	unsigned long	cr3;
	unsigned long	eip;
	unsigned long	eflags;
	unsigned long	eax,ecx,edx,ebx;
	unsigned long	esp;
	unsigned long	ebp;
	unsigned long	esi;
	unsigned long	edi;
	unsigned short	es, __esh;
	unsigned short	cs, __csh;
	unsigned short	ss, __ssh;
	unsigned short	ds, __dsh;
	unsigned short	fs, __fsh;
	unsigned short	gs, __gsh;
	unsigned short	ldt, __ldth;
	unsigned short	trace, bitmap;
	/* I/O bitmap follows.. */
};

/* For task.flags */
#define	TASK_RUNNING	1	/* Running, not suspended. */
#define TASK_FROZEN	2	/* Stopped for good. */
#define TASK_ZOMBIE	4	/* Dead, but resources not reclaimed. */
#define TASK_IMMORTAL	8	/* Can't be killed. */
#define TASK_VM		16	/* Task is a virtual machine. */

#define STD_QUANTUM	51	/* 1/20 of a second (ish) */

struct task {
    list_node_t node;

    /* The next field is hardcoded in assembly functions, don't move
       it unless you know what you're doing. */
    void (*return_hook)(struct trap_regs *regs);

    /* Low-level stuff. */
    struct tss tss;
    u_int16 tss_sel, _pad1;
    u_long pid, ppid;
    page_dir *page_dir;
    page *stack0;
    page *stack;

    /* Scheduling information. All time values are in 1024Hz ticks. */
    u_long flags;
    short pri, _pad2;
    u_long cpu_time, last_sched, quantum;
    long time_left;
    u_long sched_count;			/* Context switches to this task. */
    int forbid_count;			/* When +ve, task is non-preemptable */

    /* Misc stuff. */
    const char *name;
    struct file *current_dir;
    int errno;

    /* Exception handlers for this task. */
    void (*exceptions[8])(struct trap_regs *regs);
    void (*gpe_handler)(struct trap_regs *regs);
    bool (*pfl_handler)(struct trap_regs *regs, u_long lin_addr);

    /* Available for use by the `owner' of the task. In virtual machines
       this always points to the task's `struct vm'. */
    void *user_data;
};


/* Lists of sleeping tasks. */

struct task_list {
    struct task_list *next;
    struct task *task;
    u_long pid;				/* Used for consistency checking. */
};


#ifdef KERNEL

/* from sched.c */
extern struct task *current_task;
extern list_t running_tasks, suspended_tasks;
extern bool need_resched;
extern void append_task(struct task *task);
extern void remove_task(struct task *task);
extern void suspend_task(struct task *task);
extern void suspend_current_task(void);
extern void wake_task(struct task *task);
extern void schedule(void);
extern void add_task_list(struct task_list **head, struct task_list *elt);
extern void remove_task_list(struct task_list **head, struct task_list *elt);
extern void sleep_in_task_list(struct task_list **head);
extern void wake_up_task_list(struct task_list **head);
extern void wake_up_first_task(struct task_list **head);
extern void init_sched(void);

/* from task.c */
extern int add_initial_task(void);
extern struct task *add_task(void *task, u_long flags, short pri,
			     const char *name);
extern void reclaim_task(struct task *task);
extern int kill_task(struct task *task);
extern void kill_current_task(void);
extern struct task *find_task_by_pid(u_long pid);

struct shell;
extern void describe_tasks(struct shell *sh);

#endif /* KERNEL */


/* Disable and enable pre-emption of the current task. Note that these
   functions nest, that is if you call forbid() twice you have to sub-
   sequently call permit() twice before the task can be pre-empted. */

extern inline void
forbid(void)
{
#ifndef KERNEL
    kernel->current_task->forbid_count++;
#else
    current_task->forbid_count++;
#endif
}

extern inline void
permit(void)
{
#ifndef KERNEL
    if((--kernel->current_task->forbid_count == 0) && kernel->need_resched)
	kernel->schedule();
#else
    if((--current_task->forbid_count == 0) && need_resched)
	schedule();
#endif
}


/* Semaphores. */

struct semaphore {
    int blocked;
    struct task_list *waiting;
};

/* The current task releases its ownership of the semaphore SEM. This may
   be called from interrupts. */
extern inline void
signal(struct semaphore *sem)
{
    if(sem->waiting == NULL)
	sem->blocked = FALSE;
    else
    {
#ifdef KERNEL
	wake_up_first_task(&sem->waiting);
#else
	kernel->wake_up_first_task(&sem->waiting);
#endif
    }
}

/* Request ownership of the semaphore SEM, the current task will sleep
   until it's available. Note that this *may not* be called from an
   interrupt. */
extern inline void
wait(struct semaphore *sem)
{
    int was_blocked;
    asm ("xchgl %1,%0"
	 : "=r" (was_blocked)
	 : "m" (sem->blocked), "0" (TRUE));
    if(was_blocked)
    {
#ifdef KERNEL
	sleep_in_task_list(&sem->waiting);
#else
	kernel->sleep_in_task_list(&sem->waiting);
#endif
    }
}

/* Initialise a semaphore to be blocked. */
extern inline void
set_sem_blocked(struct semaphore *sem)
{
    sem->blocked = TRUE;
    sem->waiting = NULL;
}

/* Initialise a semaphore to be open. */
extern inline void
set_sem_clear(struct semaphore *sem)
{
    sem->blocked = FALSE;
    sem->waiting = NULL;
}



/* Switch to the task defined by the task structure TSK. Note that
   interrupts should *definitely* be masked while calling this macro. */
#define switch_to_task(tsk)			\
    asm ("ljmp %0"				\
	 : /* no output */			\
	 :"m" (*(((char *)&tsk->tss_sel)-4)))

#define ltr(tsk)				\
    asm("ltr %w0\n\t"				\
	: /* no output */			\
	:"m" (tsk))

#endif	/* __ASM__ */

#define	MAX_TASKS	16

#endif /* __TASKS_H__ */
