/* mkfs.c -- Lay down a bare file system structure.
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/time.h>
#include <vmm/string.h>
#include <vmm/bits.h>
#include <vmm/kernel.h>

#define kprintf	kernel->printf

#ifndef TEST
# define current_time kernel->current_time
#endif

#include "bootsect.h"

/* Write a bare file system onto the device DEV. DEV should *not* have
   been mounted. Note that this doesn't use the buffer-cache, it
   communicates directly with the device.

   BLOCKS is the total number of blocks in the device. RESERVED is the
   number of blocks to leave free between the boot block and the first
   inode block. */
bool
mkfs(struct fs_device *dev, u_long blocks, u_long reserved)
{
    int tmp;
    blkno block;
    blk tmp_blk;
#define TMP_INODE_BLK ((struct inode_blk *)&tmp_blk)
#define TMP_DIR_BLK ((struct dir_entry_blk *)&tmp_blk)
#define TMP_BMAP_BLK ((u_long *)&tmp_blk)
#define TMP_BOOT_BLK ((struct boot_blk *)&tmp_blk)

    /* First calculate the dimensions of each area of the disk. */
    struct super_data sup;
    sup.total_blocks = blocks;
    blocks--;				/* boot block */
    blocks -= reserved;
    sup.inode_bitmap = 1 + reserved;
    sup.num_inodes = round_to(blocks / 10, INODES_PER_BLOCK);
    sup.inodes = sup.inode_bitmap + (sup.num_inodes / (FS_BLKSIZ * 8)) + 1;
    blocks -= (((sup.num_inodes / (FS_BLKSIZ * 8)) + 1)
	       + (sup.num_inodes / INODES_PER_BLOCK));
    sup.data_bitmap = sup.inodes + (sup.num_inodes / INODES_PER_BLOCK);
    tmp = (blocks / (FS_BLKSIZ * 8)) + 1;
    blocks -= tmp;
    sup.data_size = blocks;
    sup.data = sup.data_bitmap + tmp;

    /* Now write the stuff out. */

    /* inode bitmap */
    tmp = sup.num_inodes;
    block = sup.inode_bitmap;
    memset(TMP_BMAP_BLK, 0, FS_BLKSIZ);
    while(tmp > (FS_BLKSIZ * 8))
    {
	if(block == sup.inode_bitmap)
	{
	    /* First block: mark the first inode as being used (for the
	       root directory). */
	    set_bit(TMP_BMAP_BLK, 0);
	}
	ERRNO = FS_WRITE_BLOCKS(dev, block, TMP_BMAP_BLK, 1);
	if(ERRNO < 0)
	    return FALSE;
	if(block == sup.inode_bitmap)
	    clear_bit(TMP_BMAP_BLK, 0);
	tmp -= FS_BLKSIZ * 8;
	block++;
    }
    if(block != sup.inodes)
    {
	while(tmp < (FS_BLKSIZ * 8))
	{
	    set_bit(TMP_BMAP_BLK, tmp);
	    tmp++;
	}
	if(block == sup.inode_bitmap)
	    set_bit(TMP_BMAP_BLK, 0);
	ERRNO = FS_WRITE_BLOCKS(dev, block, TMP_BMAP_BLK, 1);
	if(ERRNO < 0)
	    return FALSE;
	block++;
    }

    /* inodes, this isn't really necessary. */
    memset(TMP_INODE_BLK, 0, FS_BLKSIZ);
    while(block < sup.data_bitmap)
    {
	if(block == sup.inodes)
	{
	    /* First block, fill in the root inode. */
	    TMP_INODE_BLK->inodes[0].attr = ATTR_DIRECTORY;
	    TMP_INODE_BLK->inodes[0].modtime = current_time();
	    TMP_INODE_BLK->inodes[0].size = sizeof(struct dir_entry) * 2;
	    TMP_INODE_BLK->inodes[0].nlinks = 2;
	    TMP_INODE_BLK->inodes[0].data[0] = sup.data;
	}
	ERRNO = FS_WRITE_BLOCKS(dev, block, TMP_INODE_BLK, 1);
	if(ERRNO < 0)
	    return FALSE;
	if(block == sup.inodes)
	    memset(TMP_INODE_BLK, 0, sizeof(struct inode));
	block++;
    }

    /* data bitmap */
    tmp = sup.data_size;
    block = sup.data_bitmap;
    memset(TMP_BMAP_BLK, 0, FS_BLKSIZ);
    while(tmp > (FS_BLKSIZ * 8))
    {
	if(block == sup.data_bitmap)
	    set_bit(TMP_BMAP_BLK, 0);
	ERRNO = FS_WRITE_BLOCKS(dev, block, TMP_BMAP_BLK, 1);
	if(ERRNO < 0)
	    return FALSE;
	if(block == sup.data_bitmap)
	    clear_bit(TMP_BMAP_BLK, 0);
	tmp -= FS_BLKSIZ * 8;
	block++;
    }

    if(block != sup.data)
    {
	if(block == sup.data_bitmap)
	    set_bit(TMP_BMAP_BLK, 0);
	while(tmp < (FS_BLKSIZ * 8))
	{
	    set_bit(TMP_BMAP_BLK, tmp);
	    tmp++;
	}
	ERRNO = FS_WRITE_BLOCKS(dev, block, TMP_BMAP_BLK, 1);
	if(ERRNO < 0)
	    return FALSE;
	block++;
    }

    /* Dir entries for the root. Using the first data block as the
       root dir's first directory entry block. */
    memset(TMP_DIR_BLK, 0, FS_BLKSIZ);
    strcpy(TMP_DIR_BLK->entries[0].name, ".");
    TMP_DIR_BLK->entries[0].inum = 0;
    strcpy(TMP_DIR_BLK->entries[1].name, "..");
    TMP_DIR_BLK->entries[1].inum = 0;
    ERRNO = FS_WRITE_BLOCKS(dev, sup.data, TMP_DIR_BLK, 1);
    if(ERRNO < 0)
	return FALSE;
   
    ERRNO = FS_READ_BLOCKS(dev, BOOT_BLK, TMP_BOOT_BLK, 1);
    if(ERRNO < 0)
	return FALSE;
    memcpy(&TMP_BOOT_BLK->sup, &sup, sizeof(sup));
    TMP_BOOT_BLK->magic = FS_BOOT_MAGIC;
    memcpy(&TMP_BOOT_BLK->boot_code, bootsect_code,
        sizeof(TMP_BOOT_BLK->boot_code)); 
    ERRNO = FS_WRITE_BLOCKS(dev, BOOT_BLK, TMP_BOOT_BLK, 1);
    if(ERRNO < 0)
	return FALSE;

    return TRUE;
}
