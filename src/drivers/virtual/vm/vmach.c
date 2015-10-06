/* vmach.c -- Virtual machine handling.
   John Harper. */

#include <vmm/vm.h>
#include <vmm/tasks.h>
#include <vmm/tty.h>
#include <vmm/shell.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/fs.h>
#include <vmm/bits.h>
#include <vmm/string.h>

#define kprintf kernel->printf

/* Since VMs are often CPU-bound give them a relatively small quantum. */
#define VM_QUANTUM	(STD_QUANTUM/2)

static bool fill_vm_page_dir(struct vm *vm);

struct io_handler *global_io;
struct arpl_handler *global_arpls;

/* If TRUE unhandled I/O addresses are printed to the console when they
   are accessed. */
bool verbose_io;

/* This page is mapped into vm's to fill any regions which I don't know
   what to do with (i.e. ROMs). */
static page *empty_page;

struct vpic_module *vpic;

bool
init_vm(void)
{
    vpic = (struct vpic_module *)kernel->open_module("vpic", SYS_VER);
    if(vpic != NULL)
    {
	empty_page = kernel->alloc_page();
	memset(empty_page, 0, PAGE_SIZE);
	return TRUE;
    }
    return FALSE;
}

/* Allocate a vm structure, create a task (which is suspended) and a tty
   for it. VIRTUAL-MEM is the total amount of memory to give it in K.
   DISPLAY-TYPE is the name of the video adapter to emulate. NAME is
   used as the name of the task, it's copied. */
struct vm *
create_vm(const char *name, u_long virtual_mem, const char *display_type)
{
    struct tty_module *tty = (struct tty_module *)kernel->open_module("tty", SYS_VER);
    if(tty != NULL)
    {
	struct vm *vm = kernel->calloc(sizeof(struct vm), 1);
	if(vm != NULL)
	{
	    vm->task = kernel->add_task(NULL, TASK_VM, -1,
					kernel->strdup(name ? name : "vm"));
	    if(vm->task != NULL)
	    {
		vm->task->user_data = vm;
		vm->task->quantum = VM_QUANTUM;
		
		vm->task->exceptions[0] = vm_div_handler;
#if 0
		vm->task->exceptions[1] = vm_debug_handler;
#endif
		vm->task->exceptions[3] = vm_breakpoint_handler;
		vm->task->exceptions[4] = vm_ovfl_handler;
		vm->task->exceptions[6] = vm_invl_handler;
		vm->task->gpe_handler = vm_gpe_handler;
		vm->task->pfl_handler = vm_pfl_handler;

		vm->hardware.total_mem = virtual_mem;
		vm->hardware.base_mem = min(640, virtual_mem);
		vm->hardware.extended_mem = ((virtual_mem > 1024)
					     ? virtual_mem - 1024 : 0);

		/* This should work! The vm's memory map is filled in as normal
		   then we disable the A20 address line.. */
		vm->a20_state = TRUE;
		fill_vm_page_dir(vm);
		set_gate_a20(vm, FALSE);

		vm->tty = tty->open_tty(vm->task, Virtual, display_type, 0);
		if(vm->tty != NULL)
		{
		    /* Now fix up the tss for VM86 mode. */
		    vm->task->tss.eflags = EFLAGS_VM | FLAGS_IF | 2;
		    vm->task->tss.eip = 0;
		    vm->task->tss.cs = 0xffff;
		    vm->task->tss.ds = 0;
		    vm->task->tss.ss = 0;
		    vm->task->tss.es = 0;
		    vm->virtual_eflags = 2;
		    kernel->close_module((struct module *)tty);
		    return vm;
		}
		kernel->kill_task(vm->task);
	    }
	    kernel->free(vm);
	}
	kernel->close_module((struct module *)tty);
    }
    return NULL;
}

/* Free the vm structure VM, this involves killing its task and tty, then
   freeing VM. */
void
kill_vm(struct vm *vm)
{
    struct vm_kill_handler *kh;
    forbid();

    /* Freeze the task about to be killed.. */
    vm->task->flags |= TASK_FROZEN;
    kernel->suspend_task(vm->task);

    kh = vm->kill_list;
    while(kh != NULL)
    {
	struct vm_kill_handler *nxt = kh->next;
	if(kh->func != NULL)
	    kh->func(vm);
	kh = nxt;
    }

    if(vm->tty != NULL)
    {
	struct tty_module *tty = (struct tty_module *)kernel->open_module("tty", SYS_VER);
	if(tty != NULL)
	{
	    tty->close_tty(vm->tty);
	    kernel->close_module((struct module *)tty);
	}
	vm->tty = NULL;
    }
    if(vm->task != NULL)
    {
	if(vm->task->name)
	    kernel->free((char *)vm->task->name);
	kernel->kill_task(vm->task);
	vm->task = NULL;
    }
    permit();
    kernel->free(vm);
}

/* Fill in the page directory of the virtual machine VM. The memory size
   fields of VM->hardware define the amount of memory mapped. Note that
   no pages are allocated, the pte's are simply set up with their present
   bits unset. */
static bool
fill_vm_page_dir(struct vm *vm)
{
    page_dir *pd = vm->task->page_dir;
    u_long x = 0;
    while(x < vm->hardware.base_mem)
    {
	/* Let the page fault handler allocate and map in these
	   pages as they're used. */
	kernel->set_pte(pd, x * 1024,
			PTE_USER | PTE_READ_WRITE | PTE_FREEABLE);
	x += PAGE_SIZE / 1024;
    }
    x = 640;
    while(x < 1024)
    {
	kernel->set_pte(pd, x * 1024, TO_PHYSICAL(empty_page) |
			PTE_USER | PTE_PRESENT);
	x += PAGE_SIZE / 1024;
    }
    while(x < (1024u + vm->hardware.extended_mem))
    {
	kernel->set_pte(pd, x * 1024,
			PTE_USER | PTE_READ_WRITE | PTE_FREEABLE);
	x += PAGE_SIZE / 1024;
    }
    return TRUE;
}


/* I/O port virtualisation. */

/* Add the io-port handler IOH to the vm LOCAL if LOCAL is non-NULL, or
   to the whole system if LOCAL is NULL. */
void
add_io_handler(struct vm *local, struct io_handler *ioh)
{
    struct io_handler **head;
    forbid();
    if(local == NULL)
	head = &global_io;
    else
	head = &local->local_io;
    ioh->next = *head;
    *head = ioh;
    permit();
}

/* Remove the io-port handler IOH from the vm LOCAL if LOCAL is non-NULL, or
   from the whole system if LOCAL is NULL. */
void
remove_io_handler(struct vm *local, struct io_handler *ioh)
{
    struct io_handler **head;
    forbid();
    if(local == NULL)
	head = &global_io;
    else
	head = &local->local_io;
    while(*head != NULL)
    {
	if(*head == ioh)
	{
	    *head = ioh->next;
	    break;
	}
	head = &(*head)->next;
    }
    permit();
}

/* Return the io-port handler covering the port PORT in the vm VM, or NULL
   if that port is not virtualised. */
struct io_handler *
get_io_handler(struct vm *vm, u_short port)
{
    struct io_handler *ioh;
    forbid();
    ioh = vm->local_io;
    while(ioh != NULL)
    {
	if((port >= ioh->low_port) && (port <= ioh->high_port))
	    goto end;
	ioh = ioh->next;
    }
    ioh = global_io;
    while(ioh != NULL)
    {
	if((port >= ioh->low_port) && (port <= ioh->high_port))
	    goto end;
	ioh = ioh->next;
    }
end:
    permit();
    return ioh;
}

void
describe_vm_io(struct shell *sh, struct vm *vm)
{
    struct io_handler *ioh;
    forbid();
    if(vm == NULL)
	ioh = global_io;
    else
	ioh = vm->local_io;
    sh->shell->printf(sh, "Name\n");
    while(ioh != NULL)
    {
	sh->shell->printf(sh, "%-10s %03X-%03X\n", ioh->name,
			  ioh->low_port, ioh->high_port);
	ioh = ioh->next;
    }
    permit();
}


/* 16-bit service handling. */

/* Add the handler AH. */
void
add_arpl_handler(struct arpl_handler *ah)
{
    forbid();
    ah->next = global_arpls;
    global_arpls = ah;
    permit();
}

/* Remove the handler AH. */
void
remove_arpl_handler(struct arpl_handler *ah)
{
    struct arpl_handler **head;
    forbid();
    head = &global_arpls;
    while(*head != NULL)
    {
	if(*head == ah)
	{
	    *head = ah->next;
	    break;
	}
	head = &(*head)->next;
    }
    permit();
}

/* Return the handler covering the arpl svc, or NULL. */
struct arpl_handler *
get_arpl_handler(u_short svc)
{
    struct arpl_handler *ah;
    forbid();
    ah = global_arpls;
    while(ah != NULL)
    {
	if((svc >= ah->low) && (svc <= ah->high))
	    break;
	ah = ah->next;
    }
    permit();
    return ah;
}

void
describe_arpls(struct shell *sh)
{
    struct arpl_handler *ah;
    forbid();
    ah = global_arpls;
    sh->shell->printf(sh, "Name\n");
    while(ah != NULL)
    {
	sh->shell->printf(sh, "%-10s %03X-%03X\n", ah->name,
			  ah->low, ah->high);
	ah = ah->next;
    }
    permit();
}



void
add_vm_kill_handler(struct vm *vm, struct vm_kill_handler *kh)
{
    forbid();
    kh->next = vm->kill_list;
    vm->kill_list = kh;
    permit();
}


/* Slot handling. */

static u_long slot_bitmap;

/* Returns the index of a free slot in all virtual machine structures, or
   -1 if no free slots exist. */
int
alloc_vm_slot(void)
{
    int slot;
    forbid();
    if(slot_bitmap != 0xffffffff)
    {
	BSF(~slot_bitmap, slot);
	slot_bitmap |= 1 << slot;
    }
    else
	slot = -1;
    permit();
    return slot;
}

/* Free the slot SLOT in all virtual machines. */
void
free_vm_slot(int slot)
{
    if((slot >= 0) && (slot < 32))
	slot_bitmap &= ~(1 << slot);
}


/* A20 gate handling. */

/* When STATE is TRUE the A20 address line is enabled, thereby allowing
   `normal' access to these addresses. Otherwise the bottom 64k of the
   vm's address space is mapped at 1M. */
void
set_gate_a20(struct vm *vm, bool state)
{
    page_dir *pd;
    if(vm->a20_state == state)
	return;
    pd = vm->task->page_dir;
    if(state)
    {
	u_int i;
	DB(("vm: Enabling A20 gate for vm %u\n", vm->task->pid));
	for(i = 0; i < 16; i++)
	{
	    kernel->set_pte(pd, 0x100000 + i * PAGE_SIZE, vm->himem_ptes[i]);
	}
    }
    else
    {
	u_int i;
	DB(("vm: Disabling A20 gate for vm %u\n", vm->task->pid));
	for(i = 0; i < 16; i++)
	{
	    vm->himem_ptes[i] = kernel->get_pte(pd, 0x100000 + i * PAGE_SIZE);
	    kernel->set_pte(pd, 0x100000 + i * PAGE_SIZE,
			    kernel->get_pte(pd, i * PAGE_SIZE) & ~PTE_FREEABLE);
	}
    }
    vm->a20_state = state;
}
