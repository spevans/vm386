/* vserial.c -- Virtual Serial Ports */

/*
 * Emualate a serial port using an actual serial port. Can me mapped or
 * smiulated by a file.
 */

#define DEBUG

#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/vserial.h>
#include <vmm/string.h>

#define kprintf kernel->printf

struct kernel_module *kernel;
struct vm_module *vm;
struct fs_module *fs;

static int vm_slot;



static void delete_vserial(struct vm *vmach);
static bool create_vserial(struct vm *vmach, int argc, char **argv);
static u_long vserial_in(struct vm *vm, u_short port, int size);
static void vserial_out(struct vm *vm, u_short port, int size, u_long val);

static struct io_handler serial_io = {
    NULL, "serial", 0, 0, vserial_in, vserial_out
};


static void
delete_vserial(struct vm *vmach)
{
    struct vserial *v = vmach->slots[vm_slot];
#ifdef DEBUG
    kprintf("deleting vserial... ");
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
	vserial_module.base.vxd_base.open_count--;
    }
#ifdef DEBUG
    kprintf("done\n");
#endif
}

static bool
create_vserial(struct vm *vmach, int argc, char **argv)
{
    struct vserial *new = kernel->calloc(sizeof(struct vserial), 1);
    u_short bda_ports[4];
    if(argc == 0) return FALSE; 
    if(new != NULL)
    {
	int i;
	u_short base;

	vserial_module.base.vxd_base.open_count++;
	new->kh.func = delete_vserial;
	vm->add_vm_kill_handler(vmach, &new->kh);
	vmach->slots[vm_slot] = new;
	for(i = 0; i < 4; i++) bda_ports[i] = 0;

	for(i = 0; i < 4; i++) {
		base = kernel->strtoul(*argv, NULL, 16);
		bda_ports[i] = base;
#ifdef DEBUG
		kprintf("base address at 0x%X\n", base);
#endif
		if(base != 0) {
			new->base_addr[i] = base;
			serial_io.low_port = base;
			serial_io.high_port= base + MAX_LPR_OFFSET;
			memcpy(&(new->ports[i]), &serial_io,
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



u_short
new_spool_file(struct vm *vm, u_short port)
{
#ifdef DEBUG
  kprintf("new_spool_file on port: %d\n", port);
#endif
  return 1;
}


u_short
write_char(struct vm *vm, u_short port, u_short data)
{
#ifdef DEBUG
  kprintf("write char %2x on port: %d\n", data, port);
#endif
  return 1;
}


u_short
get_status(struct vm *vm, u_short port)
{
#ifdef DEBUG
  kprintf("get_status on port: %d\n", port);
#endif
  return 1;
}

static u_long
vserial_in(struct vm *vm, u_short port, int size)
{
    u_int offset;
    int i;
    struct vserial *v = vm->slots[vm_slot];
    if(v == NULL) return -1;
#ifdef DEBUG
    kprintf("vserial_in: 0x%X\n", port);
#endif
    for(i = 0; i < v->total_ports; i++) {
        if((port >= v->base_addr[i]) &&
           (port <= v->base_addr[i]+MAX_LPR_OFFSET)) {
            offset = port - v->base_addr[i];
            switch(offset) {
                case 0:	/* data output port */
                return (u_long)v->last_data_val[i];

                case 1: /* serial status */
                return 8;	/* on-line, no errors */

                case 2: /* serial control */
		return 0;          
            }
        }
    }
    return -1;
}

static void
vserial_out(struct vm *vm, u_short port, int size, u_long val)
{
    u_int offset;
    int i;
    struct vserial *v = vm->slots[vm_slot];
    if(v == NULL) return;
#ifdef DEBUG
    kprintf("vserial_out: 0x%X,0x%X\n", port, val);
#endif
    for(i = 0; i < v->total_ports; i++) {
        if((port >= v->base_addr[i]) &&
           (port <= v->base_addr[i]+MAX_LPR_OFFSET)) {
            offset = port - v->base_addr[i];
            switch(offset) {
                case 0:	/* data output port */
		v->last_data_val[i] = (u_short)val;
                return;	/* ignore the output for now */

                case 1: /* serial status */
                return; /* ignore writes to the status register */

                case 2: /* serial control */
		return; /* ignore for now */
            }
        }
    }
    return;
}



/* Module stuff. */

static bool
vserial_init(void)
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
vserial_expunge(void)
{
    if(vserial_module.base.vxd_base.open_count == 0)
    {
	vm->free_vm_slot(vm_slot);
	kernel->close_module((struct module *)fs);
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vserial_module vserial_module =
{
    { MODULE_INIT("vserial", SYS_VER, vserial_init, NULL, NULL, vserial_expunge),
      create_vserial},
    delete_vserial,
    new_spool_file,
    write_char,
    get_status
};
