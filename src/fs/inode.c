/* inode.c -- inode handling.
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/string.h>
#include <vmm/kernel.h>

#ifndef TEST
#define kprintf kernel->printf
#endif


/* In-core inode handling. */

static struct core_inode inode_pool[NR_INODES];
static struct core_inode *inode_free_list;
static struct core_inode *inode_open_list;

void
init_inodes(void)
{
    int i;
    for(i = 0; i < (NR_INODES - 1); i++)
	inode_pool[i].next = &inode_pool[i+1];
    inode_pool[i].next = 0;
    inode_free_list = inode_pool;
    inode_open_list = NULL;
}

void
kill_inodes(void)
{
    struct core_inode *inode;
    FORBID();
    inode = inode_open_list;
    while(inode != NULL)
    {
	if(!inode->invalid)
	    write_inode(inode);
	inode = inode->next;
    }
    PERMIT();
}

/* Return an inode structure containing inode number INUM on device
   DEV, or NULL for an error. */
struct core_inode *
make_inode(struct fs_device *dev, u_long inum)
{
    struct core_inode *inode;
    DB(("make_inode: dev=%s inum=%d inode=%p\n", dev->name, inum, inode));
    if(!test_media(dev))
	return NULL;
    FORBID();
again:
    inode = inode_open_list;
    while(inode != NULL)
    {
	if((inode->inum == inum) && (inode->dev == dev))
	{
	    if(!inode->invalid)
	    {
#ifndef TEST
		if(inode->locked)
		{
		    kernel->sleep_in_task_list(&inode->locked_tasks);
		    goto again;
		}
#endif
		inode->use_count++;
		PERMIT();
		DB(("make_inode: got cached inode, %p\n", inode));
		return inode;
	    }
	    /* We know the device is valid, so just ignore this inode;
	       there may be a valid one further down the list, otherwise
	       we'll just create a fresh one. */
	}
	inode = inode->next;
    }
    inode = inode_free_list;
    if(inode == NULL)
    {
	ERRNO = E_NOMEM;
	PERMIT();
	return NULL;
    }
    DB(("make_inode: got inode from free_list, %p free_list=%p", inode,
	inode_free_list));
    inode_free_list = inode->next;
    inode->next = inode_open_list;
    inode_open_list = inode;
    inode->use_count = 1;
    inode->dev = dev;
    inode->inum = inum;
    inode->invalid = FALSE;
    dev->use_count++;
#ifndef TEST
    inode->locked = TRUE;
#endif
    if(!read_inode(inode))
    {
	DB(("make_inode: couldn't read_inode()\n"));
	inode_open_list = inode->next;
	inode->next = inode_free_list;
	inode_free_list = inode;
	release_device(dev);
#ifndef TEST
	kernel->wake_up_task_list(&inode->locked_tasks);
#endif
	inode = NULL;
    }
    else
    {
#ifndef TEST
	inode->locked = FALSE;
	kernel->wake_up_task_list(&inode->locked_tasks);
#endif
	DB(("make_inode: attr=%#x size=%d modtime=%Z nlinks=%d\n",
	    inode->inode.attr, inode->inode.size, inode->inode.modtime,
	    inode->inode.nlinks));
    }
    PERMIT();
    return inode;
}

/* Increment the usage count of the inode INODE then return it. */
struct core_inode *
dup_inode(struct core_inode *inode)
{
    inode->use_count++;
    return inode;
}

/* Say that one person has finished with INODE. It's contents will be
   written to disk if modified and if no other references to INODE
   exist it will be freed. */
void
close_inode(struct core_inode *inode)
{
    if(inode == NULL)
	return;
    if(!inode->invalid)
	write_inode(inode);
    if(--inode->use_count == 0)
    {
	struct core_inode **x;
	FORBID();
	x = &inode_open_list;
	while(*x != inode)
	    x = &(*x)->next;
	*x = (*x)->next;
	PERMIT();

	/* Only deallocate the inode on disk when no one has it open. */
	if(!inode->invalid && inode->inode.nlinks == 0)
	{
	    delete_inode_data(inode);
	    free_inode(inode->dev, inode->inum);
	}

	release_device(inode->dev);
	FORBID();
	inode->next = inode_free_list;
	inode_free_list = inode;
	PERMIT();
    }
}

/* For every inode in memory pointing at device DEV, set its `invalid'
   flag to TRUE. Returns TRUE if no inodes point at this device, FALSE
   otherwise. */
bool
invalidate_device_inodes(struct fs_device *dev)
{
    bool status = TRUE;
    struct core_inode *inode;
    FORBID();
    inode = inode_open_list;
    while(inode != NULL)
    {
	if(inode->dev == dev)
	{
	    inode->invalid = TRUE;
	    status = FALSE;
	}
	inode = inode->next;
    }
    PERMIT();
    return status;
}


/* On-disk inode handling. */

/* Return the inode number of a free inode. Or -1 if an error occurred. */
long
alloc_inode(struct fs_device *dev)
{
    return bmap_alloc(dev, dev->sup.inode_bitmap, dev->sup.num_inodes);
}

/* Free an inode allocated by alloc_inode(). */
bool
free_inode(struct fs_device *dev, u_long inum)
{
    return bmap_free(dev, dev->sup.inode_bitmap, inum);
}

/* Refresh the contents of INODE from the disk. */
bool
read_inode(struct core_inode *inode)
{
    struct buf_head *buf;
    test_media(inode->dev);
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return FALSE;
    }
    buf = bread(inode->dev, ((inode->inum / INODES_PER_BLOCK)
			     + inode->dev->sup.inodes));
    if(buf == NULL)
	return FALSE;
    memcpy(&inode->inode,
	   &(buf->buf.inodes.inodes[inode->inum % INODES_PER_BLOCK]),
	   sizeof(struct inode));
    inode->dirty = FALSE;
    brelse(buf);
    return TRUE;
}

/* Write the inode INODE to the device DEV if it's dirty. */
bool
write_inode(struct core_inode *inode)
{
    test_media(inode->dev);
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return FALSE;
    }
    if(inode->dirty)
    {
	struct buf_head *buf = bread(inode->dev,
				     (inode->inum / INODES_PER_BLOCK)
				     + inode->dev->sup.inodes);
	if(buf == NULL)
	    return FALSE;
	memcpy(&(buf->buf.inodes.inodes[inode->inum % INODES_PER_BLOCK]),
	       &inode->inode,
	       sizeof(struct inode));
	bdirty(buf, TRUE);
	brelse(buf);
	inode->dirty = FALSE;
    }
    return TRUE;
}

static blkno
get_inode_blkno(struct core_inode *inode, int offset, bool create,
		bool *created)
{
    blkno blk = inode->inode.data[offset];
    if(blk == 0)
    {
	if(create)
	{
	    blk = alloc_block(inode->dev,
			      offset > 0 ? inode->inode.data[offset-1] : 0);
	    if(blk != 0)
	    {
		inode->inode.data[offset] = blk;
		inode->dirty = TRUE;
		if(created)
		    *created = TRUE;
	    }
	}
	else
	    ERRNO = E_NOEXIST;
    }
    else if(created)
	*created = FALSE;
    return blk;
}

static inline struct buf_head *
get_inode_blk(struct core_inode *inode, int offset, bool create, bool clr)
{
    struct buf_head *buf;
    bool created;
    blkno blk = get_inode_blkno(inode, offset, create, &created);
    if(blk == 0)
	return NULL;
    buf = bread(inode->dev, blk);
    if(buf && clr && created)
    {
	memset(&buf->buf.data, 0, FS_BLKSIZ);
	bdirty(buf, FALSE);
    }
    return buf;
}

/* Note that IND-BLK is released in here! */
static blkno
get_indirect_blkno(struct core_inode *inode, struct buf_head *ind_buf,
		   int offset, bool create, bool *created)
{
    blkno blk = ind_buf->buf.ind.data[offset];
    if(blk == 0)
    {
	if(create)
	{
	    blk = alloc_block(inode->dev,
			      (offset > 0) ? ind_buf->buf.ind.data[offset-1] : 0);
	    if(blk != 0)
	    {
		ind_buf->buf.ind.data[offset] = blk;
		bdirty(ind_buf, TRUE);
		if(created)
		    *created = TRUE;
	    }
	}
	else
	    ERRNO = E_NOEXIST;
    }
    else if(created)
	*created = FALSE;
    brelse(ind_buf);
    return blk;
}

/* Note that IND-BLK is released in here! */
static inline struct buf_head *
get_indirect_blk(struct core_inode *inode, struct buf_head *ind_buf,
		 int offset, bool create, bool clr)
{
    struct buf_head *buf;
    bool created;
    blkno blk = get_indirect_blkno(inode, ind_buf, offset, create, &created);
    if(blk == 0)
	return NULL;
    buf = bread(inode->dev, blk);
    if(buf && clr && created)
    {
	memset(&buf->buf.data, 0, FS_BLKSIZ);
	bdirty(buf, FALSE);
    }
    return buf;
}

/* Returns the data block corresponding to logical block BLK of the file
   linked to FILE. Or NULL if an error occurs. If CREATE is TRUE the block
   (and indirect links to it) will be created if they don't already exist.
   Note that if you call this with CREATE=TRUE be sure to call write_inode()
   in the near future. */
blkno
get_data_blkno(struct core_inode *inode, blkno blk, bool create)
{
    struct buf_head *tmp;
    test_media(inode->dev);
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return 0;
    }
    if(blk < SINGLE_INDIRECT)
	return get_inode_blkno(inode, blk, create, NULL);
    blk -= SINGLE_INDIRECT;
    if(blk < PTRS_PER_INDIRECT)
    {
	tmp = get_inode_blk(inode, SINGLE_INDIRECT, create, TRUE);
	if(tmp == NULL)
	    return 0;
	return get_indirect_blkno(inode, tmp, blk, create, NULL);
    }
    blk -= PTRS_PER_INDIRECT;
    if(blk < (PTRS_PER_INDIRECT * PTRS_PER_INDIRECT))
    {
	tmp = get_inode_blk(inode, DOUBLE_INDIRECT, create, TRUE);
	if(tmp == NULL)
	    return 0;
	tmp = get_indirect_blk(inode, tmp, blk / PTRS_PER_INDIRECT,
			       create, TRUE);
	if(tmp == NULL)
	    return 0;
	return get_indirect_blkno(inode, tmp, blk % PTRS_PER_INDIRECT,
				  create, NULL);
    }
    blk -= PTRS_PER_INDIRECT * PTRS_PER_INDIRECT;
    if(blk < (PTRS_PER_INDIRECT * PTRS_PER_INDIRECT * PTRS_PER_INDIRECT))
    {
	tmp = get_inode_blk(inode, TRIPLE_INDIRECT, create, TRUE);
	if(tmp == NULL)
	    return 0;
	tmp = get_indirect_blk(inode, tmp,
			       blk / (PTRS_PER_INDIRECT * PTRS_PER_INDIRECT),
			       create, TRUE);
	if(tmp == NULL)
	    return 0;
	tmp = get_indirect_blk(inode, tmp,
			       (blk / PTRS_PER_INDIRECT) % PTRS_PER_INDIRECT,
			       create, TRUE);
	if(tmp == NULL)
	    return 0;
	return get_indirect_blkno(inode, tmp, blk % PTRS_PER_INDIRECT,
				  create, NULL);
    }
    ERRNO = E_BADARG;
    return 0;
}


/* Returns the data block corresponding to logical block BLK of the file
   linked to FILE. Or NULL if an error occurs. If CREATE is TRUE the block
   (and indirect links to it) will be created if they don't already exist.
   Note that if you call this with CREATE=TRUE be sure to call write_inode()
   in the near future. */
struct buf_head *
get_data_block(struct core_inode *inode, blkno blk, bool create)
{
    blk = get_data_blkno(inode, blk, create);
    if(blk != 0)
	return bread(inode->dev, blk);
    else
	return NULL;
}
