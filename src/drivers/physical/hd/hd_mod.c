/* hd_mod.c -- The hard disk module.
   John Harper. */

#include <vmm/types.h>
#include <vmm/hd.h>
#include <vmm/kernel.h>

struct kernel_module *kernel;
struct fs_module *fs;

struct hd_module hd_module =
{
    MODULE_INIT("hd", SYS_VER, hd_init, NULL, NULL, NULL),
    hd_add_dev, hd_remove_dev,
    hd_find_partition, hd_read_blocks, hd_write_blocks,
    hd_mount_partition, hd_mkfs_partition,
};

bool
hd_init(void)
{
    fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
    if(fs != NULL)
    {
	init_partitions();
	ide_init();
	add_hd_commands();
	return TRUE;
    }
    return FALSE;
}
