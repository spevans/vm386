/* vpit.c -- Virtual Programmable Interval Timer.

   Currently this is mainly concerned with providing a 18.2 times
   per second `heartbeat'; very little else is supported..

   John Harper. */

#include <vmm/vpit.h>
#include <vmm/vm.h>
#include <vmm/vpic.h>
#include <vmm/tasks.h>
#include <vmm/kernel.h>
#include <vmm/pit.h>
#include <vmm/io.h>

#define kprintf kernel->printf

#define MIN_INTERVAL 5		/* In 1024Hz ticks. */

static bool initialised;
static bool do_init(void);
#define INITIALISED (initialised || do_init())

struct vm_module *vm;
int vpit_slot;
struct vpic_module *vpic;

struct kernel_module *kernel;

static void vpit_timer_handler(void *);



static inline u_long
get_divisor(u_short div)
{
    return div == 0 ? 65536 : div;
}

static inline u_long
calc_divisor_ticks(u_short divisor)
{
    u_long ticks = (1024 * get_divisor(divisor)) / 1193180;
    return ticks < MIN_INTERVAL ? MIN_INTERVAL : ticks;
}

static inline u_long
cmos_to_timer_ticks(u_long ticks)
{
    return ticks * (1193180 / 1024);
}

/* Note that my counters count upwards in 1024Hz ticks :) */
static inline u_long
calc_counter(struct vpit_channel *chan)
{
    u_long elapsed = kernel->get_timer_ticks() - chan->start_ticks;
    switch(chan->command & PIT_CMD_MODE)
    {
    case PIT_CMD_MODE0:
    case PIT_CMD_MODE1:
    case PIT_CMD_MODE4:
	/* One shot timers. */
	if(elapsed > chan->interval_ticks)
	    return 0;
	else
	    return chan->interval_ticks - elapsed;

    default:
	/* Continuous timers. */
	return elapsed % chan->interval_ticks;
    }
}

static inline u_long
calc_out_pin(struct vpit_channel *chan)
{
    /* Hmm.. */
    u_long counter = calc_counter(chan);
    switch(chan->command & PIT_CMD_MODE)
    {
    case PIT_CMD_MODE0:
    case PIT_CMD_MODE1:
    case PIT_CMD_MODE2:
	/* Modes that pulse when counter==0 */
	return (counter == 0) ? 1 : 0;
    case PIT_CMD_MODE3:
	{
	    /* Output is high for half the next countdown (square wave). */
	    return (counter < (get_divisor(chan->divisor) / 2)) ? 1 : 0;
	}
    default:
	kprintf("vpit:calc_out_pin: Unsupported mode bits %x\n", chan->command);
	return 0;
    }
}

static inline void
start_timer(struct vm *vm)
{
    struct vpit *vp = vm->slots[vpit_slot];
    if(vp != NULL)
    {
	u_long flags;
	save_flags(flags);
	cli();
	{
	    u_char mode = vp->channels[0].command & PIT_CMD_MODE;
	    if((mode == PIT_CMD_MODE0) || (mode == PIT_CMD_MODE2)
	       || (mode == PIT_CMD_MODE3))
	    {
		if(vp->timer_ticking)
		    kernel->remove_timer(&vp->chan0_timer);
		set_timer_interval(&vp->chan0_timer,
				    vp->channels[0].interval_ticks);
		kernel->add_timer(&vp->chan0_timer);
		vp->timer_ticking = TRUE;
	    }
	}
	load_flags(flags);
    }
}


static void
kill_vpit(struct vm *vm)
{
    struct vpit *vp = vm->slots[vpit_slot];
    if(vp != NULL)
    {
	vm->slots[vpit_slot] = NULL;
	vp->channels[0].command = 0;
	kernel->remove_timer(&vp->chan0_timer);
	kernel->free(vp);
	vpit_module.base.vxd_base.open_count--;
    }
}

static bool
create_vpit(struct vm *vmach, __attribute__ ((unused)) int argc,
            __attribute__ ((unused)) char **argv)
{
    if(INITIALISED)
    {
	struct vpit *new = kernel->calloc(1, sizeof(struct vpit));
	if(new != NULL)
	{
	    u_long cmos_time = kernel->get_timer_ticks();
	    set_timer_func(&new->chan0_timer, 0, vpit_timer_handler, vmach);
	    new->channels[0].command = PIT_CMD_LATCH_LSB_MSB | PIT_CMD_MODE3;
	    new->channels[0].divisor = 0;
	    new->channels[0].interval_ticks = calc_divisor_ticks(0);
	    new->channels[0].start_ticks = cmos_time;
	    new->channels[1].command = PIT_CMD_LATCH_LSB_MSB | PIT_CMD_MODE2;
	    new->channels[1].divisor = 18;
	    new->channels[1].interval_ticks = calc_divisor_ticks(18);
	    new->channels[1].start_ticks = cmos_time;
	    new->channels[2].command = PIT_CMD_LATCH_LSB_MSB | PIT_CMD_MODE3;
	    new->channels[2].divisor = 1331;
	    new->channels[2].interval_ticks = calc_divisor_ticks(1331);
	    new->channels[2].start_ticks = cmos_time;

	    new->kh.func = kill_vpit;
	    vm->add_vm_kill_handler(vmach, &new->kh);

	    vmach->slots[vpit_slot] = new;
	    vpit_module.base.vxd_base.open_count++;

	    /* Unmask IRQ0 in the VM */
	    vpic->set_mask(vmach, FALSE, 1<<0);

	    /* Start the timer running for IRQ0 */
	    start_timer(vmach);
	    return TRUE;
	}
    }
    return FALSE;
}


static void
vpit_out(struct vm *vm, u_short port, int size, u_long val)
{
    /* Must be INITIALISED to get here */
    struct vpit *vp = vm->slots[vpit_slot];
    DB(("vpit_out: vm=%x port=%x size=%d val=%x\n", vm, port, size, val));
    if(vp == NULL)
	return;
    if(size != 1)
	return;
    switch(port)
    {
	struct vpit_channel *chan;

    case 0x40:
    case 0x41:
    case 0x42:
	port &= 3;
	chan = &vp->channels[port];
	switch(chan->latch_state)
	{
	case counter_low:
	    chan->divisor = (chan->divisor & 0xff00) | (val & 0xff);
	    if(chan->command & PIT_CMD_LATCH_MSB)
		chan->latch_state = counter_high;
	    break;
	case counter_high:
	    chan->divisor = (chan->divisor & 0x00ff) | (val << 8);
	    if(chan->command & PIT_CMD_LATCH_LSB)
		chan->latch_state = counter_low;
	    if(port == 0)
		start_timer(vm);
	    else if(port == 2 && vp->chan2_callback)
		vp->chan2_callback(vm);
	    break;
	}
	chan->interval_ticks = calc_divisor_ticks(chan->divisor);
	chan->start_ticks = kernel->get_timer_ticks();
	break;

    case 0x43:
	{
	    if((val & PIT_CMD_READ_BACK) == PIT_CMD_READ_BACK)
	    {
		DB(("vpit: read-back val=%x\n", val));
		if(val & 0x02)
		{
		    vp->channels[0].latch_state = (((val & 0x40) == 0)
						   ? status
						   : counter_low);
		}
		if(val & 0x04)
		{
		    vp->channels[1].latch_state = (((val & 0x40) == 0)
						   ? status
						   : counter_low);
		}
		if(val & 0x08)
		{
		    vp->channels[2].latch_state = (((val & 0x40) == 0)
						   ? status
						   : counter_low);
		}
	    }
	    else
	    {
		chan = &vp->channels[(val >> 6) & 3];
		if((val & PIT_CMD_LATCH) == 0)
		{
		    chan->latch_state = counter_low;
#if 0
		    chan->command = (chan->command & ~PIT_CMD_LATCH) | val;
#endif
		}
		else
		{
		    chan->command = val;
		    if(val & PIT_CMD_LATCH_LSB)
			chan->latch_state = counter_low;
		    else if(val & PIT_CMD_LATCH_MSB)
			chan->latch_state = counter_high;
		    else
			chan->latch_state = counter_low;
		}
	    }
	    break;
	}
    }
}

static u_long
vpit_in(struct vm *vm, u_short port, int size)
{
    /* Must be INITIALISED to get here */
    struct vpit *vp = vm->slots[vpit_slot];
    DB(("vpit_in: vm=%p port=%x size=%d\n", vm, port, size));
    if(vp == NULL)
	return ~0;
    if(size != 1)
	return ~0;
    switch(port)
    {
	u_long counter;
	struct vpit_channel *chan;

    case 0x40:
    case 0x41:
    case 0x42:
	port &= 3;
	chan = &vp->channels[port];
	DB(("vpit_in: chan=%d latch_state=%d cmd=%x divisor=%x",
	    port, chan->latch_state, chan->command, chan->divisor));
	switch(chan->latch_state)
	{
	case counter_low:
	    counter = calc_divisor_ticks(chan->divisor) - calc_counter(chan);
	    if(chan->command & PIT_CMD_LATCH_MSB)
		chan->latch_state = counter_high;
	    DB(("result=%x\n", counter & 255));
	    return cmos_to_timer_ticks(counter) & 255;
	case counter_high:
	    counter = calc_divisor_ticks(chan->divisor) - calc_counter(chan);
	    if(chan->command & PIT_CMD_LATCH_LSB)
		chan->latch_state = counter_low;
	    DB(("result=%x\n", counter >> 8));
	    return cmos_to_timer_ticks(counter) >> 8;
	case status:
	    {
		u_char stat;
		if(chan->command & PIT_CMD_LATCH_MSB)
		    chan->latch_state = counter_high;
		else
		    chan->latch_state = counter_low;
		stat = (chan->command & 0x3f) | (calc_out_pin(chan) << 7);
		DB(("stat of chan%d = %x\n", port, stat));
		return stat;
	    }
	}
	break;

    case 0x43:
	DB(("vpit: read of port 0x43\n"));
	return 0;
    }
    return ~0;
}

static void
vpit_timer_handler(void *vmach)
{
    /* Must be INITIALISED to get here */
    struct vpit *vp = ((struct vm *)vmach)->slots[vpit_slot];
    if(vp != NULL)
    {
	vp->timer_ticking = FALSE;
	vpic->simulate_irq(vmach, 0);
	start_timer(vmach);
    }
}

static struct vpit *
get_vpit(struct vm *vm)
{
    return INITIALISED ? vm->slots[vpit_slot] : NULL;
}


/* Module handling. */

static struct io_handler vpit_ioh = {
    NULL, "pit", 0x40, 0x43, vpit_in, vpit_out
};

static bool
vpit_init(void)
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
	vpic = (struct vpic_module *)kernel->open_module("vpic", SYS_VER);
	if(vpic != NULL)
	{
	    vpit_slot = vm->alloc_vm_slot();
	    if(vpit_slot != -1)
	    {
		vm->add_io_handler(NULL, &vpit_ioh);
		initialised = TRUE;
		return TRUE;
	    }
	    kernel->close_module((struct module *)vpic);
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}
    
static bool
vpit_expunge(void)
{
    if(vpit_module.base.vxd_base.open_count == 0)
    {
	if(initialised)
	{
	    vm->free_vm_slot(vpit_slot);
	    vm->remove_io_handler(NULL, &vpit_ioh);
	    kernel->close_module((struct module *)vpic);
	    kernel->close_module((struct module *)vm);
	}
	return TRUE;
    }
    return FALSE;
}

struct vpit_module vpit_module = {
    { MODULE_INIT("vpit", SYS_VER, vpit_init, NULL, NULL, vpit_expunge),
      create_vpit },
    get_vpit
};
