/* cmds.c -- commands for the ramdisk module 
   Simon Evans */

#include <vmm/types.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/lists.h>
#include <vmm/ramdisk.h>

#define kprintf kernel->printf

extern list_t rd_dev_list;

#define DOC_rdinfo "fdinfo\n\
Print information about the ramdisk driver."
int
cmd_rdinfo(struct shell *sh, int argc, char *argv[])
{
    list_node_t *nxt, *x = rd_dev_list.head;
    rd_dev_t *rd;

    sh->shell->printf(sh, "Device	Ram Addr	Size\n");
    while((nxt = x->succ) != NULL)
    {
        rd = (rd_dev_t *)x;
        sh->shell->printf(sh, "%-6s %p	%u\n", rd->name, rd->ram,
            rd->total_blocks);
        x = nxt;
    }
    return 0;
}

#define DOC_addrd "addrd BLOCKS\n\
Create a new ramdisk of size BLOCKS."
int
cmd_addrd(struct shell *sh, int argc, char *argv[])
{
    u_long blocks;
    rd_dev_t *rd;

    if(argc == 0) {
        sh->shell->printf(sh, "Block size not specified\n");
        return 1;
    }
    blocks = kernel->strtoul(*argv, NULL, 10);
    if(blocks == 0) {
        sh->shell->printf(sh, "Block size to small: %u\n", blocks);
        return 1;
    }
    rd = create_ramdisk(blocks);
    if(rd == NULL) {
        sh->shell->printf(sh, "Ramdisk creation failed\n");
        return 1;
    }
    sh->shell->printf(sh, "New Ramdisk: %s\n", rd->name);
    return 0;
}

#define DOC_delrd "delrd device\n\
Remove the ramdisk 'device'."
int
cmd_delrd(struct shell *sh, int argc, char *argv[])
{
    list_node_t *nxt, *x = rd_dev_list.head;
    rd_dev_t *rd;

    if(argc == 0) {
        sh->shell->printf(sh, "No device specified\n");
        return 1;
    }
    
    while((nxt = x->succ) != NULL)
    {
        rd = (rd_dev_t *)x;
	if(!strcmp(*argv, rd->name)) {
            if(delete_ramdisk(rd) == FALSE) {
                sh->shell->printf(sh, "Cant remove `%s'\n", rd->name);
                return 1;
            } else {
                sh->shell->printf(sh, "%s removed\n", rd->name);
                return 0;
            }
        }
        x = nxt;
    }
    sh->shell->printf(sh, "No such device `%s'\n", *argv);
    return 1;
}

struct shell_cmds rd_cmds =
{
    0,
    { CMD(rdinfo), CMD(addrd), CMD(delrd), END_CMD }
};

bool
add_ramdisk_commands(void)
{
    kernel->add_shell_cmds(&rd_cmds);
    return TRUE;
}

void
remove_ramdisk_commands(void)
{
    kernel->remove_shell_cmds(&rd_cmds);
}
