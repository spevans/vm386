/* fd_mod.c -- the floppy module
 * C.Luke
 */

#include <vmm/types.h>
#include <vmm/floppy.h>
#include <vmm/kernel.h>

struct kernel_module *kernel;
struct shell_module *shell;
struct fs_module *fs;

static bool fd_init(void);
static bool fd_expunge(void);

struct fd_module fd_module =
{
	MODULE_INIT("fd", SYS_VER, fd_init, NULL, NULL, fd_expunge),
	floppy_add_dev, floppy_remove_dev,
	floppy_read_blocks, floppy_write_blocks,
	floppy_mount_partition, floppy_mkfs_partition,
	floppy_force_seek,
};

static bool fd_init(void)
{
	fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
	if(fs == NULL)
		return FALSE;

	shell = (struct shell_module *)kernel->open_module("shell", SYS_VER);
	if(shell == NULL)
	{
		kernel->close_module((struct module *)fs);
		return FALSE;
	}

	add_floppy_commands();
	floppy_init();
	return TRUE;
}

static bool fd_expunge(void)
{
	if(fd_module.base.open_count != 0)
		return FALSE;

	remove_floppy_commands();

	kernel->close_module((struct module *)shell);
	kernel->close_module((struct module *)fs);
	return TRUE;
}
