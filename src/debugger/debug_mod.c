/* debug.c -- The debugger module.
   Simon Evans */

#include <vmm/types.h>
#include <vmm/kernel.h>
#include <vmm/debug.h>

struct kernel_module *kernel;
struct shell_module *shell;

static bool debug_init(void);
static bool debug_expunge(void);

struct debug_module debug_module =
{
    MODULE_INIT("debug", SYS_VER, debug_init, NULL, NULL, debug_expunge),
    ncode,
};

static bool
debug_init(void)
{
    shell = (struct shell_module *)kernel->open_module("shell", SYS_VER);
    if(shell != NULL)
    {
	add_debug_commands();
	return TRUE;
    }
    return FALSE;
}

static bool
debug_expunge(void)
{
    if(debug_module.base.open_count != 0)
	return FALSE;
    remove_debug_commands();
    kernel->close_module((struct module *)shell);
    return TRUE;
}
