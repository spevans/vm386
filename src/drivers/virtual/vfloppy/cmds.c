/* cmds.c -- commands for the vfloppy module
   Simon Evans */

#include <vmm/types.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/lists.h>
#include <vmm/vfloppy.h>

#define kprintf kernel->printf


#define DOC_vdisk "vdisk PID FLOPPY-IMAGE\n\
Change the file used to provide the floppy image for the virtual machine
whose pid is PID."
int
cmd_vdisk(struct shell *sh, int argc, char **argv)
{
    struct task *task;
    if(argc != 2)
        return 0;
    task = kernel->find_task_by_pid(kernel->strtoul(argv[0], NULL, 0));
    if((task != NULL) && (task->flags & TASK_VM) && task->user_data)
    {
        struct vm *vmach = task->user_data;
        if(vmach != NULL)
        {
            if(change_vfloppy(vmach, argv[1]))
                return 0;
            sh->shell->perror(sh, argv[1]);
        }
    }
    return RC_FAIL;
}

struct shell_cmds vfloppy_cmds =
{
    0,
    { CMD(vdisk), END_CMD }
};
