/* IDT  and interrupt functions */

#include <vmm/segment.h>
#include <vmm/io.h>
#include <vmm/irq.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/shell.h>

void *Irq_handlers[16];
char *Irq_names[16];

static void set_idt_entry(int entry, unsigned int addr, int type)
{
	IDT[entry].lo = (KERNEL_CODE << 16) | (addr & 0xFFFF);
	IDT[entry].hi = (addr & 0xFFFF0000) | (0x8000 | type << 8);
}


void set_intr_gate(int entry, void *addr)
{
	set_idt_entry(entry, (u_long)addr, 14);
}


void set_trap_gate(int entry, void *addr)
{
	set_idt_entry(entry, (u_long)addr, 15);
}

bool alloc_irq(u_int irq, void *func, char *name)
{
	u_long flags;
	if(irq > 15) return FALSE;
	save_flags(flags);
	cli();
	if(Irq_handlers[irq] != NULL)
	{
	    load_flags(flags);
	    return FALSE;
	}
	Irq_handlers[irq] = func;
	Irq_names[irq] = name;
	if(irq <= 7) {
	    outb(inb(0x21) & ~(1 << irq), 0x21);
	} else {
	    outb(inb(0xA1) & ~(1 << (irq-7)), 0xA1);
	}
	load_flags(flags);
	return TRUE;
}


void dealloc_irq(u_int irq)
{
	if(irq > 15) return;

	if(irq <= 7) {
		outb(inb(0x21) | (1 << irq), 0x21);
	} else {
		outb(inb(0xA1) | (1 << (irq-7)), 0xA1);
	}
	Irq_names[irq] = NULL;
	Irq_handlers[irq] = NULL;
}

void init_irq_handlers()
{
	set_intr_gate(32+0, _irq0);
	set_intr_gate(32+1, _irq1);
	set_intr_gate(32+2, _irq2);
	set_intr_gate(32+3, _irq3);
	set_intr_gate(32+4, _irq4);
	set_intr_gate(32+5, _irq5);
	set_intr_gate(32+6, _irq6);
	set_intr_gate(32+7, _irq7);
	set_intr_gate(32+8, _irq8);
	set_intr_gate(32+9, _irq9);
	set_intr_gate(32+10, _irq10);
	set_intr_gate(32+11, _irq11);
	set_intr_gate(32+12, _irq12);
	set_intr_gate(32+13, _irq13);
	set_intr_gate(32+14, _irq14);
	set_intr_gate(32+15, _irq15);
}	


/* Queued interrupt handling. -- jsh. */

#define IRQ_Q_SIZ 128			/* Must be a power of 2 */

static u_char irq_queue[IRQ_Q_SIZ];
static u_int irq_q_in, irq_q_out;
static void (*irq_q_handlers[16])(void);
volatile u_int irqs_queued;
struct task *irq_q_task;

static void
queue_irq(u_int irq)
{
    register u_int new_in = (irq_q_in + 1) & (IRQ_Q_SIZ-1);
    if(new_in != irq_q_out)
    {
	irq_queue[irq_q_in] = irq;
	irq_q_in = new_in;
	irqs_queued++;
	wake_task(irq_q_task);
    }								
}

#define IRQ_Q_STUB(irq)				\
static void					\
queue_irq ## irq (void)				\
{						\
    queue_irq(irq);				\
}

IRQ_Q_STUB(0)
IRQ_Q_STUB(1)
IRQ_Q_STUB(2)
IRQ_Q_STUB(3)
IRQ_Q_STUB(4)
IRQ_Q_STUB(5)
IRQ_Q_STUB(6)
IRQ_Q_STUB(7)
IRQ_Q_STUB(8)
IRQ_Q_STUB(9)
IRQ_Q_STUB(10)
IRQ_Q_STUB(11)
IRQ_Q_STUB(12)
IRQ_Q_STUB(13)
IRQ_Q_STUB(14)
IRQ_Q_STUB(15)

static void (*irq_q_stubs[16])(void) =
{
    queue_irq0, queue_irq1, queue_irq2, queue_irq3,
    queue_irq4, queue_irq5, queue_irq6, queue_irq7,
    queue_irq8, queue_irq9, queue_irq10, queue_irq11,
    queue_irq12, queue_irq13, queue_irq14, queue_irq15
};

/* This is now a task! It runs at a high priority whenever there's
   an irq waiting to be dispatched. */
static void
service_queued_irqs(void)
{
    while(1)
    {
	while(irqs_queued != 0)
	{
	    u_char irq;
	    cli();
	    irq = irq_queue[irq_q_out++];
	    irq_q_out &= (IRQ_Q_SIZ-1);
	    irqs_queued--;
	    sti();
	    if(irq_q_handlers[irq] != NULL)
		irq_q_handlers[irq]();
	    else
		kprintf("IRQ error: no q handler for IRQ%d\n", irq);
	}
	suspend_current_task();
    }
}

bool
init_queued_irqs(void)
{
    irq_q_task = add_task(service_queued_irqs, TASK_RUNNING | TASK_IMMORTAL,
			  80, "irq-dispatcher");
    return irq_q_task ? TRUE : FALSE;
}

bool
alloc_queued_irq(u_int irq, void (*func)(void), char *name)
{
    bool rc = FALSE;
    u_long flags;
    save_flags(flags);
    cli();
    if((irq <= 15) && (irq_q_handlers[irq] == NULL))
    {
	irq_q_handlers[irq] = func;
	if(alloc_irq(irq, irq_q_stubs[irq], name))
	    rc = TRUE;
	else
	    irq_q_handlers[irq] = NULL;
    }
    load_flags(flags);
    return rc;
}

void
dealloc_queued_irq(u_int irq)
{
    if((irq > 15) || (irq_q_handlers[irq] == NULL))
	return;
    dealloc_irq(irq);
    irq_q_handlers[irq] = NULL;
}



void
describe_irqs(struct shell *sh)
{
    int i;
    sh->shell->printf(sh, "        %10s   %10s\n", "Async", "Queued");
    for(i = 0; i < 16; i++)
    {
	if(Irq_handlers[i] || irq_q_handlers[i] || Irq_names[i])
	{
	    sh->shell->printf(sh, "IRQ%-2d   %10x   %10x  %s\n", i,
			      Irq_handlers[i],
			      irq_q_handlers[i],
			      Irq_names[i] == NULL ? "" : Irq_names[i]);
	}
    }
}
