/* test_dev.c -- Simulate a device with a Unix file.
   John Harper. */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

#define __NO_TYPE_CLASHES
#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/shell.h>

int dev_fd = -1;
size_t dev_size;

static long dev_read_blocks(void *unused, blkno blk, void *buf, int count);
static long dev_write_blocks(void *unused, blkno blk, void *buf, int count);
static long dev_test_media(void *unused);

static struct fs_device *test_dev;

/* So we can simulate removing and inserting disks. */
static bool no_disk, disk_changed;

#define DOC_nodisk "nodisk\n\
Simulate removing the disk from the floppy drive."
int
cmd_nodisk(struct shell *sh, int argc, char **argv)
{
    no_disk = TRUE;
    shell->printf(sh, "Disk removed.\n");
    return RC_OK;
}

#define DOC_newdisk "newdisk\n\
Simulate inserting a disk."
int
cmd_newdisk(struct shell *sh, int argc, char **argv)
{
    if(no_disk)
	no_disk = FALSE;
    disk_changed = TRUE;
    shell->printf(sh, "Disk inserted.\n");
    return RC_OK;
}

bool
open_test_dev(const char *file, u_long blocks, bool make_fs, u_long reserved)
{
    struct stat stat_buf;
    dev_fd = open(file, O_RDWR);
    if(dev_fd < 0)
    {
#if 0
	printf("file: %s\n", file);
#endif
	perror("open_test_dev:open");
	return FALSE;
    }
    test_dev = alloc_device();
    if(test_dev == NULL)
    {
	perror("open_test_dev:alloc_device");
	return FALSE;
    }
    test_dev->name = "tst";
    test_dev->read_blocks = dev_read_blocks;
    test_dev->write_blocks = dev_write_blocks;
    test_dev->test_media = dev_test_media;
    if(blocks == 0)
    {
	if(fstat(dev_fd, &stat_buf))
	{
	    perror("fstat");
	    exit(10);
	}
	dev_size = stat_buf.st_size / FS_BLKSIZ;
    }
    else
	dev_size = blocks;
#if 0
    printf("dev_size: %d\n", dev_size);
#endif
    if(!make_fs || mkfs(test_dev, dev_size, reserved))
    {
	add_device(test_dev);
	shell->add_command("nodisk", cmd_nodisk, DOC_nodisk);
	shell->add_command("newdisk", cmd_newdisk, DOC_newdisk);
	return TRUE;
    }
    else
	fprintf(stderr, "Can't mkfs.\n");
    remove_device(test_dev);
    close(dev_fd);
    dev_fd = -1;
    return FALSE;
}

void
close_test_dev(void)
{
    remove_device(test_dev);
    close(dev_fd);
    dev_fd = -1;
    shell->remove_command("newdisk");
    shell->remove_command("nodisk");
}

static long
dev_read_blocks(void *unused, blkno blk, void *buf, int count)
{
    long actual;
    if(no_disk)
	return E_NODISK;
    if(lseek(dev_fd, blk * FS_BLKSIZ, SEEK_SET) < 0)
    {
	perror("read_block:lseek");
	abort();
    }
    actual = read(dev_fd, buf, count * FS_BLKSIZ);
    if(actual < 0)
    {
	if(errno == EIO)
	    return E_IO;
	perror("read_block:read");
	abort();
    }
    if(actual < FS_BLKSIZ)
	return E_IO;
    return actual;
}

static long
dev_write_blocks(void *unused, blkno blk, void *buf, int count)
{
    long actual;
    if(no_disk)
	return E_NODISK;
    if(lseek(dev_fd, blk * FS_BLKSIZ, SEEK_SET) < 0)
    {
	perror("write_block:lseek");
	abort();
    }
    actual = write(dev_fd, buf, count * FS_BLKSIZ);
    if(actual < 0)
    {
	if(errno == EIO)
	    return E_IO;
	else if(errno == ENOSPC)
	    return E_NOSPC;
	perror("write_block:write");
	abort();
    }
    if(actual < FS_BLKSIZ)
	return E_IO;
    return actual;
}

static long
dev_test_media(void *unused)
{
    if(disk_changed)
    {
	no_disk = FALSE;
	disk_changed = FALSE;
	return E_DISKCHANGE;
    }
    else if(no_disk)
	return E_NODISK;
    return E_OK;
}
