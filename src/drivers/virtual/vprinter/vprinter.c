/* vprinter.c -- Virtual Printer Ports */

/*
 * basic emulation - Capture data sent to ports and spool to a file
 * Use a timeout between writes to seperate file output. Also handle
 * bios emulation
 */

#define DEBUG

#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/vprinter.h>
#include <vmm/fs.h>
#include <vmm/string.h>

#define kprintf kernel->printf

static struct vm_module *vm;
static struct fs_module *fs;

static int vm_slot;



static void delete_vprinter(struct vm *vmach);
static bool create_vprinter(struct vm *vmach, int argc, char **argv);
static u_long vprinter_in(struct vm *vm, u_short port, int size);
static void vprinter_out(struct vm *vm, u_short port, int size, u_long val);

static struct io_handler printer_io = {
    NULL, "printer", 0, 0, vprinter_in, vprinter_out
};


static void
delete_vprinter(struct vm *vmach)
{
    struct vprinter *v = vmach->slots[vm_slot];
#ifdef DEBUG
    kprintf("deleting vprinter... ");
#endif
    if(v != NULL)
    {
	int i;
	for(i = 0; i < 4; i++) {
		if(v->base_addr[i] != 0) {
			if(v->spool_file[i] != NULL) {
#ifdef DEBUG
    kprintf("closing spool file for port: %d...", i);
#endif
                            fs->close(v->spool_file[i]);
#ifdef DEBUG
    kprintf("done\n");
#endif
                        }
			vm->remove_io_handler(vmach, &(v->ports[i]));
		}
	}
	vmach->slots[vm_slot] = NULL;
	kernel->free(v);
	vprinter_module.base.vxd_base.open_count--;
    }
#ifdef DEBUG
    kprintf("done\n");
#endif
}

static bool
create_vprinter(struct vm *vmach, int argc, char **argv)
{
    struct vprinter *new = kernel->calloc(sizeof(struct vprinter), 1);
    //u_short bda_ports[4];
    if(argc == 0) return FALSE; 
    if(new != NULL)
    {
	int i;
	u_short base;

	vprinter_module.base.vxd_base.open_count++;
	new->kh.func = delete_vprinter;
	vm->add_vm_kill_handler(vmach, &new->kh);
	vmach->slots[vm_slot] = new;
	//for(i = 0; i < 4; i++) bda_ports[i] = 0;

	for(i = 0; i < 4; i++) {
		base = kernel->strtoul(*argv, NULL, 16);
		//bda_ports[i] = base;
#ifdef DEBUG
		kprintf("base address at 0x%X\n", base);
#endif
		if(base != 0) {
			new->base_addr[i] = base;
			printer_io.low_port = base;
			printer_io.high_port= base + MAX_LPR_OFFSET;
			memcpy(&(new->ports[i]), &printer_io,
				sizeof(struct io_handler));
			vm->add_io_handler(vmach, &(new->ports[i]));
			vmach->hardware.lprports[i] = base;
			new->last_data_val[i] = 0;
			new->total_ports++;
		}
		argc--;
		argv++;
	}
	new->vm = vmach;
        return TRUE;
    }
    return FALSE;
}



static void
printer_write_char (struct vm *vm, struct vm86_regs *regs)
{
        DB(("printer_write_char vm = %p regs = %p", vm, regs));
}

static void
printer_initialise (struct vm *vm, struct vm86_regs *regs)
{
        DB(("printer_initialise vm = %p regs = %p", vm, regs));
}

static void
printer_get_status (struct vm *vm, struct vm86_regs *regs)
{
        DB(("printer_get_status vm = %p regs = %p", vm, regs));
}


u_short
new_spool_file(struct vm *vm, u_short port)
{
        DB(("new_spool_file on vm = %p port: %d\n", vm, port));
        return 1;
}


u_short
write_char(struct vm *vm, u_short port, u_short data)
{
        DB(("vprinter: write_char vm = %p port = %u data = %up", vm, port, data));
  return 1;
}


u_short
get_status(struct vm *vm, u_short port)
{
    DB(("vprinter: get_status vm = %p port = %p", vm, port));
  return 1;
}

static u_long
vprinter_in(struct vm *vm, u_short port, __attribute__ ((unused)) int size)
{
    u_int offset;
    size_t i;

    struct vprinter *v = vm->slots[vm_slot];
    if(v == NULL) return -1;
#ifdef DEBUG
    kprintf("vprinter_in: 0x%X\n", port);
#endif
    for(i = 0; i < v->total_ports; i++) {
        if((port >= v->base_addr[i]) &&
           (port <= v->base_addr[i]+MAX_LPR_OFFSET)) {
            offset = port - v->base_addr[i];
            switch(offset) {
                case 0:	/* data output port */
                return (u_long)v->last_data_val[i];

                case 1: /* printer status */
                return 8;	/* on-line, no errors */

                case 2: /* printer control */
		return 0;          
            }
        }
    }
    return -1;
}

static void
vprinter_out(struct vm *vm, u_short port, __attribute__ ((unused)) int size, u_long val)
{
    u_int offset;
    size_t i;

    struct vprinter *v = vm->slots[vm_slot];
    if(v == NULL) return;
#ifdef DEBUG
    kprintf("vprinter_out: 0x%X,0x%X\n", port, val);
#endif
    for(i = 0; i < v->total_ports; i++) {
        if((port >= v->base_addr[i]) &&
           (port <= v->base_addr[i]+MAX_LPR_OFFSET)) {
            offset = port - v->base_addr[i];
            switch(offset) {
                case 0:	/* data output port */
		v->last_data_val[i] = (u_short)val;
                return;	/* ignore the output for now */

                case 1: /* printer status */
                return; /* ignore writes to the status register */

                case 2: /* printer control */
		return; /* ignore for now */
            }
        }
    }
    return;
}



/* Module stuff. */

static bool
vprinter_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
	fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
	if(fs != NULL) {
            vm_slot = vm->alloc_vm_slot();
            if(vm_slot >= 0)
                return TRUE;
	    kernel->close_module((struct module *)fs);
        }
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vprinter_expunge(void)
{
    if(vprinter_module.base.vxd_base.open_count == 0)
    {
	vm->free_vm_slot(vm_slot);
	kernel->close_module((struct module *)fs);
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vprinter_module vprinter_module =
{
    { MODULE_INIT("vprinter", SYS_VER, vprinter_init, NULL, NULL, vprinter_expunge),
      create_vprinter},
    delete_vprinter,
    printer_write_char,
    printer_initialise,
    printer_get_status
};
