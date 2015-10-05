/* buffer.c -- Simple buffer-cache.
   John Harper.  */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/string.h>

#ifndef TEST
# define kprintf kernel->printf
#else
# define kprintf printf
#include <stdio.h>
#endif


static list_t buffer_table[BUFFER_BUCKETS];
static struct buf_head *bh_free_list;
static struct buf_head buffer_pool[NR_BUFFERS];

/* Some simple statistics. */
u_long total_accessed, cached_accesses, dirty_accesses;

static bool handle_device_error(struct buf_head *bh, int access_type,
				int errno);

void
init_buffers(void)
{
    int i;
    for(i = 0; i < BUFFER_BUCKETS; i++)
	init_list(&buffer_table[i]);
    for(i = 0; i < (NR_BUFFERS - 1); i++)
	buffer_pool[i].link.next_free = &buffer_pool[i+1];
    buffer_pool[i].link.next_free = NULL;
    bh_free_list = buffer_pool;
}

void
kill_buffers(void)
{
    int i;
    for(i = 0; i < NR_BUFFERS; i++)
    {
	if(buffer_pool[i].dirty)
	{
	    FS_WRITE_BLOCKS(buffer_pool[i].dev, buffer_pool[i].blkno,
			    &buffer_pool[i].buf.data, 1);
	}
    }
}

/* This should be called in the middle of a forbid(). */
static inline struct buf_head *
find_buffer(struct fs_device *dev, blkno blk)
{
    struct buf_head *nxt, *x;
    x = (struct buf_head *)buffer_table[BUFFER_HASH(blk)].head;
    while((nxt = (struct buf_head *)x->link.node.succ) != NULL)
    {
	if((x->blkno == blk) && (x->dev == dev) && !x->invalid)
	    return x;
	x = nxt;
    }
    return NULL;
}

/* Try to move an unreferenced but cached buffer from the hash queues
   to the free list of buffers. Returns TRUE if there *may* be a buffer
   available (no guarantee), FALSE if there definitely isn't.
   This function MAY sleep. */
static bool
make_free_buffer(void)
{
    /* Try to flush an unused buffer, otherwise we have to fail :-( */
    struct buf_head *x;
    int tries = 0;
    FORBID();
    while(tries++ < BUFFER_BUCKETS)
    {
	static int last_bucket;
	struct buf_head *nxt;
	last_bucket = (last_bucket + 1) % BUFFER_BUCKETS;
	x = (struct buf_head *)buffer_table[last_bucket].tailpred;
	while((nxt = (struct buf_head *)x->link.node.pred) != NULL)
	{
	    if(x->use_count == 0)
	    {
		remove_node(&x->link.node);
		if(x->dirty)
		{
		    long result = FS_WRITE_BLOCKS(x->dev, x->blkno,
						  &x->buf, 1);
		    if((result < 0)
		       || !handle_device_error(x, F_WRITE, -result))
		    {
			kprintf("buffer_cache: Can't write block %d to device %s\n",
				x->blkno, x->dev->name);
		    }
		    dirty_accesses++;
		}
		x->link.next_free = bh_free_list;
		bh_free_list = x;
		PERMIT();
		return TRUE;
	    }
	    x = nxt;
	}
    }
    PERMIT();
    ERRNO = E_NOMEM;
    return FALSE;
}

/* Return a buffer containing the block BLKNO of the device DEV, or NULL if
   an error occurred or there's no free buffers. This function MAY sleep. */
struct buf_head *
bread(struct fs_device *dev, blkno blk)
{
    struct buf_head *x;
    DB(("bread(`%s', %d)\n", dev->name, blk));
    if(!test_media(dev))
	return NULL;
    total_accessed++;
    FORBID();
again:
    x = find_buffer(dev, blk);
    if(x != NULL)
    {
#ifndef TEST
	if(x->locked)
	{
	    /* Another task is waiting on this buffer. Sleep until it
	       wakes. */
	    kernel->sleep_in_task_list(&x->locked_tasks);
	    goto again;
	}
#endif
	x->use_count++;
	/* Move x to the head of the list to show it was recently used. */
	remove_node(&x->link.node);
	prepend_node(&buffer_table[BUFFER_HASH(blk)], &x->link.node);
	cached_accesses++;
    }
    else
    {
	/* Couldn't find a cached copy of the buffer. Make a new one. */
	long result;
	if((x = bh_free_list) == NULL)
	{
	    if(make_free_buffer())
		goto again;
	    else
	    {
		PERMIT();
		return NULL;
	    }
	}
	bh_free_list = x->link.next_free;
	prepend_node(&buffer_table[BUFFER_HASH(blk)], &x->link.node);
	x->dev = dev;
	x->blkno = blk;
	x->use_count = 1;
	x->dirty = FALSE;
	x->invalid = FALSE;
#ifndef TEST
	/* This signals that any other tasks wanting this block should wait
	   until we've finished reading the block. Effectively the tasks
	   are synchronised. */
	x->locked = TRUE;
#endif
	result = FS_READ_BLOCKS(dev, blk, &x->buf, 1);
	if((result < 0) && !handle_device_error(x, F_READ, -result))
	{
	    remove_node(&x->link.node);
#ifndef TEST
	    kernel->wake_up_task_list(&x->locked_tasks);
#endif
	    x->link.next_free = bh_free_list;
	    bh_free_list = x;
	    x = NULL;
	}
	else
	{
#ifndef TEST
	    x->locked = FALSE;
	    kernel->wake_up_task_list(&x->locked_tasks);
#endif
	}
    }
    PERMIT();
    return x;
}

/* Write the FS_BLKSIZ bytes at DATA to the block number BLK of device DEV
   in a way compatible with the buffer cache. Returns FALSE if an error
   occurred. */
bool
bwrite(struct fs_device *dev, blkno blk, const void *data)
{
    struct buf_head *x;
    DB(("bwrite(`%s', %d)\n", dev->name, blk));
    if(!test_media(dev))
	return FALSE;
    total_accessed++;
    FORBID();
again:
    x = find_buffer(dev, blk);
    if(x != NULL)
    {
#ifndef TEST
	if(x->locked)
	{
	    /* Another task is waiting on this buffer. Sleep until it
	       wakes. */
	    kernel->sleep_in_task_list(&x->locked_tasks);
	    goto again;
	}
#endif
	x->use_count++;
	/* Move x to the head of the list to show it was recently used. */
	remove_node(&x->link.node);
	prepend_node(&buffer_table[BUFFER_HASH(blk)], &x->link.node);
	cached_accesses++;
    }
    else
    {
	/* No cached version of this buffer. */
	if((x = bh_free_list) == NULL)
	{
	    if(make_free_buffer())
		goto again;
	    else
	    {
		PERMIT();
		return FALSE;
	    }
	}
	bh_free_list = x->link.next_free;
	prepend_node(&buffer_table[BUFFER_HASH(blk)], &x->link.node);
	x->dev = dev;
	x->blkno = blk;
	x->use_count = 1;
	x->invalid = FALSE;
#ifndef TEST
	x->locked = FALSE;
#endif
    }
    memcpy(&x->buf, data, FS_BLKSIZ);
    x->dirty = TRUE;
    PERMIT();
    brelse(x);
    return TRUE;
}

/* Mark that the contents of the buffer BH has been modified since it
   was returned from bread(). If WRITE-NOW is TRUE the contents of the
   block will be written to its device immediately. */
void
bdirty(struct buf_head *bh, bool write_now)
{
    test_media(bh->dev);
    if(bh->invalid)
	return;
    bh->dirty = TRUE;
    if(write_now)
    {
	long result;
	/* Have to clear this hear in case any other tasks come along and
	   dirty the buffer while we're writing it. */
	bh->dirty = FALSE;
	result = FS_WRITE_BLOCKS(bh->dev, bh->blkno, &bh->buf.data, 1);
	if((result < 0) && !handle_device_error(bh, F_WRITE, -result))
	    bh->dirty = TRUE;
    }
}

/* Release your hold on the buffer BH. */
void
brelse(struct buf_head *bh)
{
    test_media(bh->dev);
    /* Don't decrement use_count until we're really finished with it. */
    if(bh->use_count == 1)
    {
	if(bh->invalid)
	{
	    FORBID();
	    remove_node(&bh->link.node);
	    bh->link.next_free = bh_free_list;
	    bh_free_list = bh;
	    PERMIT();
	    return;
	}
    }
#ifndef LAZY_WRITES
    if(bh->dirty)
    {
	long result;
	/* see bdirty() */
	bh->dirty = FALSE;
	result = FS_WRITE_BLOCKS(bh->dev, bh->blkno, &bh->buf.data, 1);
	if((result < 0) && !handle_device_error(bh, F_WRITE, -result))
	    bh->dirty = TRUE;
    }
#endif
    bh->use_count--;
}

/* Flush all cached blocks from the device DEV. If DONT-WRITE is TRUE
   then this function isn't allowed to write to the device (presumably
   because the media was changed), note that this may lead to cached
   writes going missing... */
void
flush_device_cache(struct fs_device *dev, bool dont_write)
{
    int i;
    FORBID();
    for(i = 0; i < NR_BUFFERS; i++)
    {
	if(buffer_pool[i].dev == dev)
	{
	    buffer_pool[i].invalid = TRUE;
	    if(buffer_pool[i].dirty && !dont_write)
	    {
		buffer_pool[i].dirty = FALSE;
		FS_WRITE_BLOCKS(buffer_pool[i].dev, buffer_pool[i].blkno,
			       &buffer_pool[i].buf.data, 1);
	    }
	}
    }
    PERMIT();
}

/* This get's called when one of the buffer routines calls dev->read_block()
   or dev->write_block() and gets an error. This function examines the
   error code and sees what should be done. If possible it will repeat the
   device access and return TRUE if it succeeded, FALSE if nothing could
   be done about the error.
   ACCESS-TYPE is either F_READ or F_WRITE showing what kind of access the
   error occurred on (from the buffer BH).
   ERRNO is the error type -- it will be put into the task's errno field. */
static bool
handle_device_error(struct buf_head *bh, int access_type, int errno)
{
    ERRNO = errno;
    if(errno == E_NODISK)
    {
	/* Some git removed the floppy, invalidate everything pointing
	   to it. */
	invalidate_device(bh->dev);
    }
    return FALSE;
}

/* Most, if not all, file system functions should call this before doing
   anything to a inode (file) to check if that inode is still valid.
   Returns TRUE if it's ok to proceed, FALSE means don't access the
   device. If it detects that the media changed or was removed, all
   cached buffers and inodes, as well as the device itself are
   marked as being invalid. */
bool
test_media(struct fs_device *dev)
{
    if(dev->test_media == NULL)
	return TRUE;
    switch(dev->test_media(dev))
    {
    case E_NODISK:
	if(!dev->invalid)
	{
	    /* We thought the device was OK. Invalidate it and any
	       cached buffers/inodes. */
	    invalidate_device(dev);
	}
	ERRNO = E_NODISK;
	return FALSE;

    case E_DISKCHANGE:
	if(!dev->invalid)
	    invalidate_device(dev);
	ERRNO = E_DISKCHANGE;
	return validate_device(dev);
    }
    return !dev->invalid;
}
