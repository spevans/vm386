/* test.c -- provides testing of other modules */

#include <vmm/kernel.h>
#include <vmm/shell.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"



/* Commands stuff */

#define DOC_test_fs "test_fs OPTIONS...\n\
Test the file system.\n\
Available options are:\n\n"
int
cmd_test_fs(struct shell *sh, int argc, char **argv)
{
    return 0;
}

struct shell_cmds test_cmds =
{
    0,
    { CMD(test_fs), END_CMD }
};

bool
add_test_cmds(void)
{
    kernel->add_shell_cmds(&test_cmds);
    return TRUE;
}


bool test_init(void)
{
    add_test_cmds();
    return TRUE;
}
