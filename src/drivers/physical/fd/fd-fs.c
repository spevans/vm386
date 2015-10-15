/* fd-fs.c -- Floppy driver interface to the fs module.
 * C.Luke
 */

#define DEBUG
#include "fd.h"


long floppy_read_blocks(fd_dev_t *fd, void *buf, u_long block, int count)
{
	blkreq_t req;
	req.buf = buf;
	req.command = FD_CMD_READ;
	req.block = block;
	req.nblocks = count;
	req.dev = (fd_dev_t *)fd;
	return fd_sync_request(&req);
}


long floppy_write_blocks(fd_dev_t *fd, void *buf, u_long block, int count)
{
	blkreq_t req;
	req.buf = buf;
	req.command = FD_CMD_WRITE;
	req.block = block;
	req.nblocks = count;
	req.dev = (fd_dev_t *)fd;
	return fd_sync_request(&req);
}

#define FACTOR (FS_BLKSIZ / FD_SECTSIZ)

long floppy_read_block(void *fd, blkno block, void *buf, int count)
{
	return floppy_read_blocks((fd_dev_t *)fd,
		buf, block * FACTOR, count * FACTOR) ? 1 : -E_IO;
}


long floppy_write_block(void *fd, blkno block, void *buf, int count)
{
	return floppy_write_blocks((fd_dev_t *)fd,
		buf, block * FACTOR, count * FACTOR) ? 1 : -E_IO;
}


long floppy_test_media(void *f)
{
#if 1
	fd_dev_t *fd = (fd_dev_t *)f;
	DB(("fd: testing media of: %s\n", fd->name));
	return E_NODISK;
#else
	DB(("fd: testing media!\n"));
	return E_NODISK;
#endif
}

long floppy_force_seek(__attribute__ ((unused)) fd_dev_t *fd, u_long cyl)
{
	blkreq_t req;
	req.buf = NULL;
	req.command = FD_CMD_SEEK;
	req.block = cyl;
	req.nblocks = 0;
	req.dev = &fd_devs[0];

	return fd_sync_request(&req);
}


bool floppy_add_dev(fd_dev_t *fd) 
{
	struct fs_device *dev;
        struct fs_module *fs = kernel->get_fs_module();

	start_motor(fd);
	fdc_specify(fd);
	fdc_recal(fd);

	if(fs == NULL) {
		kprintf("fd: Can't mount %s, fs not initialised\n", fd->name);
			return FALSE;
	}
	dev = fs->alloc_device();
	if(dev != NULL) {
		dev->name = fd->name;
		dev->read_blocks = floppy_read_block;
		dev->write_blocks = floppy_write_block;
		dev->test_media = floppy_test_media;
		dev->user_data = fd;
		if(fs->add_device(dev)) {
			kprintf("%s added\n", fd->name);
		} else {
			fs->remove_device(dev);
			return FALSE;
		}
	}
	return TRUE;
}


bool floppy_remove_dev(void)
{
	return FALSE;
}


bool floppy_mount_partition(void)
{
	return FALSE;
}


bool floppy_mkfs_partition(void)
{
	return FALSE;
}

/* Timer.
 * Used to turn of floppy motors when idle, mostly.
 */
void timer_intr(__attribute__ ((unused)) void *user_data)
{
	if(int_timer_flag == FALSE)
		return;

	set_timer_func(&timer, FD_TIMEINT, timer_intr, NULL);
	kernel->add_timer(&timer);

	stop_motor_test();
}

