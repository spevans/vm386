/* ramdisk_mod.c -- The ramdisk disk module.
   Simon Evans. */

#include <vmm/types.h>
#include <vmm/ramdisk.h>
#include <vmm/kernel.h>


struct ramdisk_module ramdisk_module =
{
    MODULE_INIT("ramdisk", SYS_VER, ramdisk_init, NULL, NULL, NULL),
    ramdisk_add_dev, ramdisk_remove_dev,
    ramdisk_read_blocks, ramdisk_write_blocks,
    ramdisk_mount_disk, ramdisk_mkfs_disk,
    create_ramdisk,
    delete_ramdisk,
    add_ramdisk_commands
};
