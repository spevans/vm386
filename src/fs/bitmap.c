/* bitmap.c -- Bitmap handling. 
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/bits.h>
#include <vmm/io.h>
#include <vmm/kernel.h>
#ifndef TEST
# define kprintf kernel->printf
#endif


/* Generic disk-block-based bitmap handling. */

/* Find a free entity in the bitmap starting at block BMAP-START on device
   DEV which contains BMAP-LEN bits. Sets the free bit it finds then returns
   the bit number, or -1 if an error or all bits are set. */
long
bmap_alloc(struct fs_device *dev, blkno bmap_start, u_long bmap_len)
{
    blkno bmap_blk = bmap_start;
    while(bmap_len > 0)
    {
	int len, bit;
	struct buf_head *buf = bread(dev, bmap_blk);
	if(buf == NULL)
	    return -1;
	len = min(bmap_len, FS_BLKSIZ * 8);
	FORBID();
	bit = find_zero_bit(buf->buf.bmap, len);
	if(bit != -1)
	{
	    set_bit(buf->buf.bmap, bit);
	    PERMIT();
	    bdirty(buf, TRUE);
	    brelse(buf);
	    return ((bmap_blk - bmap_start) * FS_BLKSIZ * 8) + bit;
	}
	PERMIT();
	brelse(buf);
	bmap_len -= len;
	bmap_blk++;
    }
    ERRNO = E_NOSPC;
    return -1;
}

/* Mark the entity at bit number BIT of the bitmap starting at block
   BMAP-START of device DEV. Return TRUE if no errors occurred. */
bool
bmap_free(struct fs_device *dev, blkno bmap_start, u_long bit)
{
    blkno bmap_blk = (bit / (FS_BLKSIZ * 8)) + bmap_start;
    struct buf_head *buf = bread(dev, bmap_blk);
    if(buf == NULL)
	return FALSE;
    bit = bit % (FS_BLKSIZ * 8);
    if(!test_bit(buf->buf.bmap, bit))
    {
	kprintf("fs: Oops, freeing a free bit (%u) in bitmap %u\n",
		bit, bmap_start);
    }
    else
    {
	clear_bit(buf->buf.bmap, bit);
	bdirty(buf, TRUE);
    }
    brelse(buf);
    return TRUE;
}


/* Data-block bitmap handling. */

/* Allocate a new block from DEV's bitmap. LOCALITY is where you want the
   new block to be close to. Note that LOCALITY is ignored for now... */
blkno
alloc_block(struct fs_device *dev, blkno locality)
{
    blkno blk = bmap_alloc(dev, dev->sup.data_bitmap, dev->sup.data_size);
    return (blk == -1) ? 0 : dev->sup.data + blk;
}

/* Deallocate the block BLK from DEV. */
bool
free_block(struct fs_device *dev, blkno blk)
{
    return bmap_free(dev, dev->sup.data_bitmap, blk - dev->sup.data);
}


/* Returns the total number of used blocks in the device DEV. This takes
   into account the boot-block, inode-blocks and the bitmap-blocks. */
u_long
used_blocks(struct fs_device *dev)
{
    u_long total = 0;
    blkno bmap_blk = dev->sup.data_bitmap;
    u_long bmap_len = dev->sup.data_size;	/* in bits */
    while(bmap_len > 0)
    {
	int i, len;
	struct buf_head *buf = bread(dev, bmap_blk);
	if(buf == NULL)
	    return 0;
	len = min(bmap_len, FS_BLKSIZ * 8);
	for(i = 0; i < len; i ++)
	{
	    if(test_bit(buf->buf.bmap, i))
		total++;
	}
	brelse(buf);
	bmap_len -= len;
	bmap_blk++;
    }
    return (total + 1
	    + (dev->sup.num_inodes / (FS_BLKSIZ * 8))
	    + (dev->sup.num_inodes / INODES_PER_BLOCK)
	    + (dev->sup.data_size / (FS_BLKSIZ * 8)));
}
