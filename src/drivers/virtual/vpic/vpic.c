/* vpic.c -- Simple PIC virtualisation.

   Currently this only supports a limited set of the 8259A's features, also
   I suspect that some bits are just plain wrong..

   John Harper. */

#include <vmm/vpic.h>
#include <vmm/tasks.h>
#include <vmm/bits.h>
#include <vmm/kernel.h>
#include <vmm/traps.h>

#define kprintf kernel->printf

static bool initialised;
static bool do_init(void);
#define INITIALISED (initialised || do_init())

struct vm_module *vm;
int vpic_slot;

struct kernel_module *kernel;


/* Virtual device stuff. */

static void
kill_vpic(struct vm *vm)
{
    struct vpic_pair *pic = vm->slots[vpic_slot];
    if(pic != NULL)
    {
	kernel->free(pic);
	vm->slots[vpic_slot] = NULL;
	vpic_module.base.vxd_base.open_count--;
    }
}

static bool
create_vpic(struct vm *vmach, int argc, char **argv)
{
    if(INITIALISED)
    {
	struct vpic_pair *new = kernel->calloc(1, sizeof(struct vpic_pair));
	if(new != NULL)
	{
	    new->master.state = new->slave.state = Normal;
	    new->master.base = 0x08;
	    new->slave.base = 0x70;
	    /* Mask out all IRQs */
	    new->master.mask = new->slave.mask = 0xFF;
	    new->master.is_slave = FALSE;
	    new->slave.is_slave = TRUE;

	    new->kh.func = kill_vpic;
	    vm->add_vm_kill_handler(vmach, &new->kh);

	    vmach->slots[vpic_slot] = new;
	    vpic_module.base.vxd_base.open_count++;
	    return TRUE;
	}
    }
    return FALSE;
}


/* Guts of the VPIC */

/* This gets put into the task->return_hook when virtual interrupts are
   pending. It's called with interrupts masked. */
static void
vpic_return_hook(struct trap_regs *regs)
{
    /* Must be INITIALISED to get here */
    struct vm *vmach = kernel->current_task->user_data;
    struct vpic_pair *pair = vmach->slots[vpic_slot];
    if((regs->eflags & EFLAGS_VM)
       && (pair != NULL)
       && (vmach->virtual_eflags & FLAGS_IF)
       && (pair->master.isr == 0)
       && (pair->master.irr != 0))
    {
	int irq;
	BSF((u_long)pair->master.irr, irq);
	pair->master.irr &= ~(1 << irq);
	pair->master.isr = 1 << irq;
	if(pair->master.link == (1 << irq))
	{
	    /* Cascaded IRQ */
	    int sirq;
	    BSF((u_long)pair->slave.irr, sirq);
	    pair->slave.irr &= ~(1 << sirq);
	    pair->slave.isr = 1 << sirq;
	    if(pair->slave.irr != 0)
	    {
		pair->master.irr = ((pair->master.irr
				     | 1 << pair->slave.link)
				    & ~pair->master.mask);
	    }
	    vm->simulate_int(vmach, regs, pair->slave.base + sirq);
	}
	else
	    vm->simulate_int(vmach, regs, pair->master.base + irq);
    }
    if(pair != NULL && pair->master.irr == 0)
	vmach->task->return_hook = NULL;
}

static inline void
install_return_hook(struct vm *vm)
{
    /* Must be INITIALISED to get here */
    struct vpic_pair *pair = vm->slots[vpic_slot];
    if((pair != NULL)
       && (vm->virtual_eflags & FLAGS_IF)
       && (pair->master.irr != 0))
    {
	vm->task->return_hook = vpic_return_hook;
    }
}

static void
IF_enabled(struct vm *vm)
{
    if(INITIALISED)
    {
	u_long flags;
	save_flags(flags);
	cli();
	install_return_hook(vm);
	load_flags(flags);
    }
}

static void
IF_disabled(struct vm *vm)
{
    if(INITIALISED)
    {
	u_long flags;
	save_flags(flags);
	cli();
	vm->task->return_hook = NULL;
	load_flags(flags);
    }
}

static void
set_mask(struct vm *vm, bool set, u_short mask)
{
    if(INITIALISED)
    {
	struct vpic_pair *pair = vm->slots[vpic_slot];
	if(pair != NULL)
	{
	    if(set)
	    {
		pair->master.mask |= (mask & 0xff);
		pair->slave.mask |= (mask >> 8);
	    }
	    else
	    {
		pair->master.mask &= (~mask & 0xff);
		pair->slave.mask &= ((~mask >> 8) & 0xff);
	    }
	}
    }
}

static void
simulate_irq(struct vm *vm, u_char irq)
{
    DB(("simulate_irq: vm=%p pair=%p irq=%d\n", vm, vm->slots[vpic_slot], irq));
    if(INITIALISED)
    {
	struct vpic_pair *pair = vm->slots[vpic_slot];
	if(pair != NULL)
	{
	    struct vpic *pic = (irq >= 8) ? &pair->slave : &pair->master;
	    u_long flags;
	    save_flags(flags);
	    cli();
	    if(irq >= 8)
		irq -= 8;
	    pic->irr = (pic->irr | (1 << irq)) & ~pic->mask;
	    if(pic->is_slave)
	    {
		pair->master.irr = ((pair->master.irr | 1 << pic->link)
				    & ~pair->master.mask);
	    }
	    if(vm->virtual_eflags & FLAGS_IF)
	    {
		if(vm->hlted)
		{
		    /* Wake the task, on its way out of the the
		       vm_gpe_handler() it should get the interrupt
		       frame pushed onto its stack.. */
		    kernel->wake_task(vm->task);
		    vm->hlted = FALSE;
		}
		vm->task->return_hook = vpic_return_hook;
	    }
	    load_flags(flags);
	}
    }
}


static void
vpic_out(struct vm *vm, u_short port, int size, u_long val)
{
    /* Must be INITIALISED to get here */
    struct vpic_pair *pair = vm->slots[vpic_slot];
    if(pair == NULL)
	return;
    if(size == 1)
    {
	struct vpic *pic = (port & 0x80) ? &pair->slave : &pair->master;
	switch(port)
	{
	case 0x20:
	case 0xA0:
	    if(pic->state == Normal)
	    {
		if(val & 0x10)
		{
		    /* ICW1 */
		    DB(("vpic_out: ICW1=%x\n", val));
		    pic->state = WaitICW2;
		    pic->need_icw4 = val & 0x01;
		    if(val & 0x0A)
			kprintf("vpic: ICW1 has unsupported modes\n");
		}
		else if((val & 0x18) == 0)
		{
		    /* OCW2 */
		    DB(("vpic_out: OCW2=%x\n", val));
		    switch(val & 0xF8)
		    {
		    case 0x20:
			/* Non-specific EOI */
		    case 0x60:
			/* Specific EOI */
			pic->isr = 0;
			install_return_hook(vm);
			break;
		    case 0x40:
			/* NOP */
			break;
		    default:
			kprintf("vpic: unsupported OCW2 bits (%x)\n", val);
		    }
		}
		else if((val & 0x18) == 0x08)
		{
		    /* OCW3 */
		    DB(("vpic_out: OCW3=%x\n", val));
		    if(val & 0x02)
			pic->state = (val & 1) ? ReadIRR : ReadISR;
		    else
			kprintf("vpic: unsupported OCW3 bits (%x)\n", val);
		}
	    }
	    break;

	case 0x21:
	case 0xA1:
	    if(pic->state == Normal)
	    {
		/* OCW1 */
		DB(("vpic_out: OCW1=%x\n", val));
		pic->mask = val;
	    }
	    else
	    {
		switch(pic->state)
		{
		case WaitICW2:
		    DB(("vpic_out: ICW2=%x\n", val));
		    pic->base = val & 7;
		    pic->state = WaitICW3;
		    break;
		case WaitICW3:
		    DB(("vpic_out: ICW3=%x\n", val));
		    if(pic->is_slave)
			pic->link = val & 7;
		    else
			pic->link = val;
		    pic->state = pic->need_icw4 ? WaitICW4 : Normal;
		    break;
		case WaitICW4:
		    DB(("vpic_out: ICW4=%x\n", val));
		    if(val & 0x1A)
			kprintf("vpic: unsupported bits in ICW4 (%x)\n", val);
		    pic->state = Normal;
		    break;
		default:
		}
	    }
	    break;
	}
    }
}

static u_long
vpic_in(struct vm *vm, u_short port, int size)
{
    /* Must be INITIALISED to get here */
    long res = -1;
    struct vpic_pair *pair = vm->slots[vpic_slot];
    if(pair == NULL)
	return ~0;
    if(size == 1)
    {
	switch(port)
	{
	case 0x20:
	    if(pair->master.state == ReadISR)
		res = pair->master.isr;
	    else if(pair->master.state == ReadIRR)
		res = pair->master.irr;
	    pair->master.state = Normal;
	    break;

	case 0x21:
	    res = pair->master.mask;
	    break;

	case 0xA0:
	    if(pair->slave.state == ReadISR)
		res = pair->slave.isr;
	    else if(pair->slave.state == ReadIRR)
		res = pair->slave.irr;
	    pair->slave.state = Normal;
	    break;

	case 0xA1:
	    res = pair->slave.mask;
	    break;
	}
    }
    return res;
}


/* Module stuff. */

static struct io_handler master_ioh = {
    NULL, "low-pic", 0x20, 0x21, vpic_in, vpic_out
};
static struct io_handler slave_ioh = {
    NULL, "high-pic", 0xA0, 0xA1, vpic_in, vpic_out
};

static bool
vpic_init(void)
{
    initialised = FALSE;
    return TRUE;
}

static bool
do_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
	vpic_slot = vm->alloc_vm_slot();
	if(vpic_slot != -1)
	{
	    vm->add_io_handler(NULL, &master_ioh);
	    vm->add_io_handler(NULL, &slave_ioh);
	    initialised = TRUE;
	    return TRUE;
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vpic_expunge(void)
{
    if(vpic_module.base.vxd_base.open_count == 0)
    {
	if(initialised)
	{
	    vm->free_vm_slot(vpic_slot);
	    vm->remove_io_handler(NULL, &slave_ioh);
	    vm->remove_io_handler(NULL, &master_ioh);
	    kernel->close_module((struct module *)vm);
	}
	return TRUE;
    }
    return FALSE;
}

struct vpic_module vpic_module = {
    { MODULE_INIT("vpic", SYS_VER, vpic_init, NULL, NULL, vpic_expunge),
      create_vpic },
    simulate_irq, IF_enabled, IF_disabled, set_mask
};
