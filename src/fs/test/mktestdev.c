/* mktestdev.c -- Write a big file to disk.
   John Harper. */

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#define __NO_TYPE_CLASHES
#include <vmm/fs.h>

int
main(int argc, char **argv)
{
    u_long blocks;
    if(argc != 3)
    {
	puts("usage: mktestdev FILE-NAME BLOCKS");
	return 1;
    }
    blocks = atoi(argv[2]);
    if(blocks > 0)
    {
	int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd >= 0)
	{
	    ftruncate(fd, blocks * FS_BLKSIZ);
	    close(fd);
	    return 0;
	}
	perror(argv[1]);
    }
    else
	puts("Need a non-zero BLOCKS argument.");
    return 1;
}
