/* glue.c -- Code to create virtual machines, using many of the vxx modules.
   John Harper. */

#include <vmm/vm.h>
#include <vmm/tasks.h>
#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/tty.h>

#define kprintf kernel->printf

struct init_vm {
    struct init_vm *next;
    struct shell *sh;
    struct vm *vm;
};

static struct init_vm *init_vm_list;

static struct init_vm *
find_init_vm(struct shell *sh)
{
    struct init_vm *iv;
    forbid();
    iv = init_vm_list;
    while(iv != NULL)
    {
	if(iv->sh == sh)
	    break;
	iv = iv->next;
    }
    permit();
    return iv;
}

static struct init_vm *
unlink_init_vm(struct shell *sh)
{
    struct init_vm **ptr;
    forbid();
    ptr = &init_vm_list;
    while(*ptr != NULL)
    {
	if((*ptr)->sh == sh)
	{
	    struct init_vm *init = *ptr;
	    *ptr = (*ptr)->next;
	    permit();
	    return init;
	}
	ptr = &(*ptr)->next;
    }
    permit();
    return NULL;
}

#define DOC_vminit "vminit [NAME] [MEM-SIZE] [DISPLAY-TYPE]\n\
Creates a new virtual machine, ready for initialisation. NAME is the name\n\
to give the task running the new virtual machine, MEM-SIZE is the number\n\
of kilobytes of memory to give it (including the ROM/Video RAM area).\n\
DISPLAY-TYPE is the type of virtual display adaptor to give it, currently\n\
either MDA or CGA."
int
cmd_vminit(struct shell *sh, int argc, char **argv)
{
    char name_buf[100];
    u_long mem = (argc > 1) ? kernel->strtoul(argv[1], NULL, 0) : 1024;
    const char *display = (argc > 2) ? argv[2] : "mda";
    struct init_vm *init = kernel->malloc(sizeof(struct init_vm));
    if(init == NULL)
	return RC_FAIL;
    init->sh = sh;
    kernel->sprintf(name_buf, "vm<%s>", (argc > 0) ? argv[0] : "");
    init->vm = create_vm(name_buf, mem, display);
    if(init->vm != NULL)
    {
	forbid();
	init->next = init_vm_list;
	init_vm_list = init;
	permit();
	init->vm->hardware.monitor_type = !strcasecmp(display, "mda") ? 3 : 2;
	return 0;
    }
    sh->shell->printf(sh, "Error: Can't create virtual machine\n");
    return RC_FAIL;
}

#define DOC_vmlaunch "vmlaunch\n\
Terminate the virtual machine initialisation section started by the most\n\
recent vminit command, then start the machine running."
int
cmd_vmlaunch(struct shell *sh, int argc, char **argv)
{
    struct init_vm *init = unlink_init_vm(sh);
    if(init != NULL)
    {
	kernel->wake_task(init->vm->task);
	kernel->free(init);
	return 0;
    }
    sh->shell->printf(sh, "Error: not inside a `vm-init' section\n");
    return RC_FAIL;
}

#define DOC_vmvxd "vmvxd MODULE-NAME [ARGS...]\n\
Install the virtual device called MODULE-NAME into the virtual machine being\n\
initialised by the current `vminit' section."
int
cmd_vmvxd(struct shell *sh, int argc, char **argv)
{
    int rc = RC_FAIL;
    struct init_vm *init = find_init_vm(sh);
    if(init != NULL)
    {
	if(argc > 0)
	{
	    struct vxd_module *mod =
		(struct vxd_module *)kernel->open_module(argv[0], SYS_VER);
	    if(mod != NULL)
	    {
		if(mod->create_vxd(init->vm, argc - 1, argv + 1))
		    rc = 0;
		kernel->close_module((struct module *)mod);
	    }
	    else
		sh->shell->printf(sh, "Error: Can't open module `%s'\n", argv[0]);
	}
	else
	    sh->shell->printf(sh, "Error: no module specified\n");
    }
    else
	sh->shell->printf(sh, "Error: not inside a `vm-init' section\n");
    return rc;
}

struct shell_cmds vm_glue_cmds =
{
    0, { CMD(vminit), CMD(vmlaunch), CMD(vmvxd), END_CMD }
};

void
add_glue_commands(void)
{
    kernel->add_shell_cmds(&vm_glue_cmds);
}
