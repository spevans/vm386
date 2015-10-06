/* test.c -- Virtual machine shell commands.
   John Harper. */

#include <vmm/vm.h>
#include <vmm/fs.h>
#include <vmm/shell.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>

#define DOC_vminfo "vminfo OPTIONS...\n\
Prints text describing the current state of the VM environment. Options\n\
available are:\n\n\
	`-io [PID]'	Print virtual I/O handlers.\n\
	`-arpl'		Print arpl handlers."
int
cmd_vminfo(struct shell *sh, int argc, char **argv)
{
    while(argc > 0)
    {
	if(!strcmp("-io", *argv))
	{
	    if(argc > 1 && argv[1][0] != '-')
	    {
		struct task *task =
		    kernel->find_task_by_pid(kernel->strtoul(argv[1],
							     NULL, 0));
		if((task != NULL) && (task->flags & TASK_VM))
		    describe_vm_io(sh, task->user_data);
		else
		    sh->shell->printf(sh, "Error: no vm %s\n", argv[1]);
		argv++; argc--;
	    }
	    else
		describe_vm_io(sh, NULL);
	}
	else if(!strcmp("-arpl", *argv))
	    describe_arpls(sh);
	else
	    sh->shell->printf(sh, "Error: unknown option `%s'\n", *argv);
	argc--; argv++;
    }
    return 0;
}

#define DOC_dbio "dbio [-on] [-off]\n\
Controls whether or not accesses of unhandled virtual I/O ports are\n\
logged to the console as they occur."
int
cmd_dbio(struct shell *sh, int argc, char **argv)
{
    if(argc != 1) {
        sh->shell->printf(sh, "Error: missing option\n");
    } else {
        if(!strcmp("-on", *argv)) {
	    verbose_io = TRUE;
        } else if(!strcmp("-off", *argv)) {
            verbose_io = FALSE;
        } else {
            sh->shell->printf(sh, "Error: unknown option `%s'\n", *argv);
        }
    }
    return 0;
}

struct shell_cmds vm_cmds =
{
    0,
    { CMD(vminfo), CMD(dbio), END_CMD }
};

bool
add_vm_commands(void)
{
    kernel->add_shell_cmds(&vm_cmds);
    return TRUE;
}
