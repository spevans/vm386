/* wbb.c -- Write the boot sector code into a disk image.
   John Harper. */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define __NO_TYPE_CLASHES
#include <vmm/fs.h>

int
main(int argc, char **argv)
{
    int disk_fd, bb_fd;
    struct boot_blk bb_bb, disk_bb;
    if(argc != 3)
    {
	puts("Usage: wbb BOOT-FILE DISK-IMAGE-FILE");
	exit(1);
    }
    bb_fd = open(argv[1], O_RDONLY);
    if(bb_fd < 0)
	goto bb_error;
    if(read(bb_fd, &bb_bb, sizeof(bb_bb)) != sizeof(bb_bb))
    {
    bb_error:
	perror(argv[1]);
	exit(10);
    }
    close(bb_fd);
    disk_fd = open(argv[2], O_RDWR);
    if(disk_fd < 0)
    {
	perror(argv[2]);
	exit(10);
    }
    if(read(disk_fd, &disk_bb, sizeof(disk_bb)) != sizeof(disk_bb))
	goto disk_error;
    memcpy(&bb_bb.sup, &disk_bb.sup, sizeof(struct super_data));
    lseek(disk_fd, 0, SEEK_SET);
    if(write(disk_fd, &bb_bb, sizeof(bb_bb)) != sizeof(bb_bb))
    {
    disk_error:
	perror(argv[1]);
	exit(10);
    }
    close(disk_fd);
    return 0;
}
