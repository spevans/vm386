/* sched.c -- High-level task handling.
   John Harper. */

#include <vmm/tasks.h>
#include <vmm/irq.h>
#include <vmm/io.h>
#include <vmm/kernel.h>
#include <vmm/time.h>

#define PARANOID

/* The currently executing task. */
struct task *current_task;

/* Lists of running and suspended tasks. Note that these should only
   be accessed with interrupts masked. */
static list_t running_tasks, suspended_tasks;

/* List of tasks terminated but not reclaimed. */
static list_t zombie_tasks;
static int zombies;

/* Flag saying when we should call schedule(). This is mirrored in the
   variable kernel->need_resched. */
bool need_resched;


/* Scheduling etc... */

#if 0
static void
print_list(list_t *task_list)
{
    struct task *x = (struct task *)task_list->head;
    while(x->node.succ != NULL)
    {
	kprintf(" %x", x);
	x = (struct task *)x->node.succ;
    }
}	
static void
print_queues(char *msg)
{
    kprintf("%s: running_tasks <", msg);
    print_list(&running_tasks);
    kprintf(" > suspended_tasks <");
    print_list(&suspended_tasks);
    kprintf(" >\n");
}
#endif

/* Insert the task TASK into the list of tasks, LIST, according to its
   priority. Note that TASKs with a non-zero time_left value are put
   before those with a zero time-left (in the same priority-band). */
static void
enqueue_task(list_t *list, struct task *task)
{
    struct task *x = (struct task *)list->head;
    if(task->time_left <= 0)
    {
	while((x->node.succ != NULL) && (x->pri >= task->pri))
	    x = (struct task *)x->node.succ;
    }
    else
    {
	while((x->node.succ != NULL) && (x->pri > task->pri))
	    x = (struct task *)x->node.succ;
	/* Found the start of our priority band, now skip any tasks
	   with time left. */
	while((x->node.succ != NULL) && (x->pri == task->pri)
	      && (x->time_left > 0))
	    x = (struct task *)x->node.succ;
    }
    if(x->node.pred != NULL)
	x = (struct task *)x->node.pred;
    /* Now X points to the node before where we need to wedge in the
       task. */
    insert_node(list, &task->node, &x->node);
}

/* Add the task TASK to the correct queue, running_tasks if it's
   runnable, suspended_tasks otherwise. */
void
append_task(struct task *task)
{
    u_long flags;
    save_flags(flags);
    cli();
    if(task->flags & TASK_RUNNING)
    {
	enqueue_task(&running_tasks, task);
	if(current_task->pri < task->pri)
	{
	    /* A higher priority task ready to run always gets priority. */
	    if(intr_nest_count == 0)
		schedule();
	    else
		need_resched = kernel_module.need_resched = TRUE;
	}
    }
    else
	append_node(&suspended_tasks, &task->node);
    load_flags(flags);
}

/* Remove the task TASK from the scheduler's data structures. */
void
remove_task(struct task *task)
{
    u_long flags;
    save_flags(flags);
    cli();
    task->flags |= TASK_ZOMBIE;
    task->flags &= ~TASK_RUNNING;
    remove_node(&task->node);
    load_flags(flags);
}

/* If the task TASK is running, set it's state to suspended and put it
   into the list of suspended tasks. This function may be called from
   interrupts. */
void
suspend_task(struct task *task)
{
    if(task->flags & TASK_RUNNING)
    {
	u_long flags;
	save_flags(flags);
	cli();
	remove_node(&task->node);
	append_node(&suspended_tasks, &task->node);
	task->flags &= ~TASK_RUNNING;
	need_resched = kernel_module.need_resched = TRUE;
	load_flags(flags);
    }
}

/* Suspend the current task immediately, note that this *may not* be
   called from an interrupt handler. */
void
suspend_current_task(void)
{
    forbid();
    suspend_task(current_task);
    schedule();
    permit();
}

/* If the task TASK is suspended change its state to running and move it to
   the end of the run queue. This function may be called from interrupts. */
void
wake_task(struct task *task)
{
    if((task->flags & (TASK_RUNNING | TASK_FROZEN | TASK_ZOMBIE)) == 0)
    {
	u_long flags;
	save_flags(flags);
	cli();
	remove_node(&task->node);
	task->flags |= TASK_RUNNING;
	enqueue_task(&running_tasks, task);
	if(task->pri > current_task->pri)
	{
	    if(intr_nest_count == 0)
		schedule();
	    else
		need_resched = kernel_module.need_resched = TRUE;
	}
	load_flags(flags);
    }
}

/* The scheduler; if possible, switch to the next task in the run queue.

   Note that the only reason to *ever* call this function is when the current
   task has suspended itself and needs to actually stop executing. Otherwise
   just set the `need_resched' flag to TRUE and the scheduler will be called
   as soon as is safe.

   Never ever *ever* call this from an interrupt handler! It should be safe
   to be called from an exception handler though.

   Also note that there are no `sliding' priority levels; tasks with high
   priority levels can totally block lower-priority tasks.  */
void
schedule(void)
{
    u_long flags;
    save_flags(flags);
    cli();

#ifdef PARANOID
    if(intr_nest_count != 0)
	kprintf("schedule: Oops, being called with intr_nest_count=%lu\n",
		intr_nest_count);
#endif

    /* First reclaim any dead processes.. */
    while(zombies)
    {
	struct task *zombie = (struct task *)zombie_tasks.head;
	remove_node(&zombie->node);
	reclaim_task(zombie);
	zombies--;
    }

    if((current_task->forbid_count > 0)
       && (current_task->flags & TASK_RUNNING))
    {
	/* Non pre-emptible task. */
	load_flags(flags);
	return;
    }

    need_resched = kernel_module.need_resched = FALSE;

    /* Now do the scheduling.. */
    if(current_task->flags & TASK_RUNNING)
    {
	/* Task is still runnable so put it onto the end of the run
	   queue (paying attention to priority levels). */
	remove_node(&current_task->node);
	enqueue_task(&running_tasks, current_task);
    }
    if(!list_empty_p(&running_tasks))
    {
	struct task *next = (struct task *)running_tasks.head;
	if(next->time_left <= 0)
	    next->time_left = next->quantum;
	if(current_task != next)
	{
	    current_task->cpu_time += timer_ticks - current_task->last_sched;
	    if(current_task->flags & TASK_ZOMBIE)
	    {
		append_node(&zombie_tasks, &current_task->node);
		zombies++;
	    }
	    next->sched_count++;
	    next->last_sched = timer_ticks;
	    current_task = next;
	    kernel_module.current_task = next;
	    switch_to_task(next);
#if 1
	    /* Currently we don't handle the math-copro *at all*; clearing
	       this flag simply stops us getting dna exceptions.. */
	    asm volatile ("clts");
#endif
	}
    }
    else
	kprintf("schedule: No task to run!?\n");
    load_flags(flags);
}


/* Task-list handling. */

/* Add the task in the task-list element ELT to the tail of the list of tasks
   pointed to by HEAD. */
void
add_task_list(struct task_list **head, struct task_list *elt)
{
    u_long flags;
    save_flags(flags);
    cli();
    while(*head != NULL)
	head = &(*head)->next;
    elt->next = *head;
    *head = elt;
    load_flags(flags);
}

/* Remove the task contained in the element ELT from the list of tasks
   pointed to by HEAD. */
void
remove_task_list(struct task_list **head, struct task_list *elt)
{
    u_long flags;
    save_flags(flags);
    cli();
    while(*head != elt)
	head = &(*head)->next;
    *head = (*head)->next;
    load_flags(flags);
}

/* Put the current task to sleep in the list of tasks pointed to by HEAD.
   Note that since this calls schedule() it may not be called by IRQ
   handlers. */
void
sleep_in_task_list(struct task_list **head)
{
    struct task_list tl;
    forbid();
    tl.task = current_task;
    tl.pid = current_task->pid;
    add_task_list(head, &tl);
    suspend_task(current_task);
    schedule();
    permit();
}

/* Wake up all tasks contained in the task-list pointed to by HEAD then
   set the list to be empty. */
void
wake_up_task_list(struct task_list **head)
{
    u_long flags;
    struct task_list *t;
    save_flags(flags);
    cli();
    intr_nest_count++;
    t = *head;
    while(t != NULL)
    {
	if(t->task->pid == t->pid)
	    wake_task(t->task);
	t = t->next;
    }
    *head = NULL;
    intr_nest_count--;
    load_flags(flags);
}

/* Remove the first element in the list of tasks pointed to by HEAD then
   wake up the task contained in that element. */
void
wake_up_first_task(struct task_list **head)
{
    u_long flags;
    struct task_list *t;
    save_flags(flags);
    cli();
    intr_nest_count++;
    t = *head;
    if(t != NULL)
    {
	*head = t->next;
	if(t->task->pid == t->pid)
	    wake_task(t->task);
    }
    intr_nest_count--;
    load_flags(flags);
}


void
init_sched(void)
{
    init_list(&running_tasks);
    init_list(&suspended_tasks);
    init_list(&zombie_tasks);
}
