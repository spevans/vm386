/* fs_mod.c -- File system's module
   John Harper. */

#include <vmm/fs.h>
#include <vmm/kernel.h>
#include <vmm/string.h>

struct fs_module fs_module = {
    MODULE_INIT("fs", SYS_VER, fs_init, NULL, NULL, NULL),

    /* Device functions. */
    alloc_device, add_device, remove_device, get_device, release_device,

    /* Filesystem functions. */
    create_file, open_file, close_file, read_file, write_file, seek_file,
    dup_file, truncate_file, set_file_size, make_link, remove_link,
    set_file_modes, make_directory, remove_directory, get_current_dir,
    swap_current_dir, make_symlink, mkfs,

    /* Buffer-cache functions. */
    bread, bwrite, bdirty, brelse,

    /* Library functions. */
    fs_putc, fs_getc, fs_read_line, fs_write_string, fs_fvprintf, fs_fprintf,
};

struct kernel_module *kernel;
char root_dir[40];


bool
fs_init(void)
{
/* first of all, get our root device */
#ifndef TEST
    strcpy(root_dir, kernel->root_dev);
    if(root_dir[0] == '\0')
	strcpy(root_dir, ":");
    kernel->printf("Root dev: %s\n", root_dir);
#endif
    init_devices();
    init_buffers();
    init_inodes();
    init_files();
    add_fs_commands();
    return TRUE;
}

void
fs_kill(void)
{
    kill_files();
    kill_inodes();
    kill_buffers();
}
