/* file.c -- file handling.
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/time.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/io.h>

#ifndef TEST
# define kprintf kernel->printf
# define current_time kernel->current_time
#endif

static struct file file_pool[NR_FILES];
static struct file *file_free_list;

void
init_files(void)
{
    int i;
    for(i = 0; i < (NR_FILES - 1); i++)
	file_pool[i].next = &file_pool[i+1];
    file_pool[i].next = 0;
    file_free_list = file_pool;
}

void
kill_files(void)
{
}

/* Return a new file structure, pointing at the inode INODE. Returns
   NULL in case of an error.  */
struct file *
make_file(struct core_inode *inode)
{
    struct file *file;
    if(!test_media(inode->dev))
	return NULL;
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return NULL;
    }
    FORBID();
    if((file = file_free_list) == NULL)
    {
	PERMIT();
	ERRNO = E_NOMEM;
	return NULL;
    }
    file_free_list = file->next;
    PERMIT();
    file->next = NULL;
    file->inode = dup_inode(inode);
    file->mode = F_READ | F_WRITE;
    file->pos = 0;
    return file;
}

/* Lose the file structure FILE. */
void
close_file(struct file *file)
{
    if(file != NULL)
    {
	test_media(file->inode->dev);
	close_inode(file->inode);
	FORBID();
	file->next = file_free_list;
	file_free_list = file;
	PERMIT();
    }
}

/* Create and return a new file structure, pointing at the same file as
   FILE. Returns NULL for an error. */
struct file *
dup_file(struct file *file)
{
    struct file *new;
    if(file == NULL)
    {
	ERRNO = E_BADARG;
	return NULL;
    }
    new = make_file(file->inode);
    if(new != NULL)
	new->mode = file->mode;
    return new;
}

/* Change the current position of the file FILE. TYPE says how to interpret
   ARG; options are SEEK_ABS for an absolute position, SEEK_REL to add ARG
   to the current position, or SEEK_EOF to move ARG bytes from the end of
   the file. Returns the new position or a negative error code. */
long
seek_file(struct file *file, long arg, int type)
{
    long new_pos;
    if(file == NULL)
	return -(ERRNO = E_BADARG);
    if(!test_media(file->inode->dev))
	return -ERRNO;
    if(file->inode->invalid)
	return -(ERRNO = E_INVALID);
    switch(type)
    {
    case SEEK_ABS:
	new_pos = arg;
	break;

    case SEEK_REL:
	new_pos = file->pos + arg;
	break;

    case SEEK_EOF:
	new_pos = file->inode->inode.size - arg;
	break;

    default:
	return -(ERRNO = E_BADARG);
    }
    if((new_pos < 0) || (new_pos > file->inode->inode.size))
	return -(ERRNO = E_BADARG);
    else
	return file->pos = new_pos;
}

/* Read LEN bytes from FILE into BUF. Either the number of bytes actually
   read, or a negative error code is returned. */
long
read_file(void *buf, size_t len, struct file *file)
{
    long actual = 0;
    DB(("read_file: buf=%p len=%Z file=%p (inode=%d)\n", buf, len, file,
	file->inode->inum));
    if(file == NULL)
	return -(ERRNO = E_BADARG);
    if(!(file->mode & F_READ))
	return -(ERRNO = E_PERM);
    if(!test_media(file->inode->dev))
	return -ERRNO;
    if(file->inode->invalid)
	return -(ERRNO = E_INVALID);
    while((len > 0) && (file->pos < file->inode->inode.size))
    {
	struct buf_head *blk;
	long this_read = min(len, FS_BLKSIZ - (file->pos % FS_BLKSIZ));
	if(file->pos + this_read > file->inode->inode.size)
	    this_read = file->inode->inode.size - file->pos;
	DB(("read_file: this_read=%d pos=%d\n", this_read, file->pos));
	blk = get_data_block(file->inode, file->pos / FS_BLKSIZ, FALSE);
	if(blk == NULL)
	{
	    if(ERRNO == E_NOEXIST)
	    {
		DB(("read_file: filling in sparse block\n"));
		/* The block doesn't exist; i.e. it's a sparse file
		   and we can assume it would contain zeros. */
		memset(buf, 0, this_read);
	    }
	    else
		return (actual > 0) ? actual : -ERRNO;
	}
	else
	{
	    memcpy(buf, &blk->buf.data[file->pos % FS_BLKSIZ], this_read);
	    brelse(blk);
	}
	buf += this_read;
	len -= this_read;
	actual += this_read;
	file->pos += this_read;
    }
    DB(("read_file: read %d bytes\n", actual));
    return actual;
}

/* Write LEN bytes from BUF to FILE. Either the number of bytes actually
   written, or a negative error code is returned. */
long
write_file(const void *buf, size_t len, struct file *file)
{
    long actual = 0;
    if(file == NULL)
	return -(ERRNO = E_BADARG);
    if(!(file->mode & F_WRITE))
	return -(ERRNO = E_PERM);
    if(!test_media(file->inode->dev))
	return -ERRNO;
    if(file->inode->invalid)
	return -(ERRNO = E_INVALID);
    while(len > 0)
    {
	long this_write = min(len, FS_BLKSIZ - (file->pos % FS_BLKSIZ));
	if(this_write == FS_BLKSIZ)
	{
	    /* A whole block; use bwrite() to save unnecessary block
	       reads. */
	    blkno blk = get_data_blkno(file->inode, file->pos / FS_BLKSIZ, TRUE);
	    if((blk == 0) || !bwrite(file->inode->dev, blk, buf))
		goto error;
	}
	else
	{
	    struct buf_head *blk = get_data_block(file->inode,
						  file->pos / FS_BLKSIZ, TRUE);
	    if(blk != NULL)
	    {
		memcpy(&blk->buf.data[file->pos % FS_BLKSIZ], buf, this_write);
		bdirty(blk, FALSE);
		brelse(blk);
	    }
	    else
	    {
	    error:
		if(actual == 0)
		    actual = -ERRNO;
		goto end;
	    }
	}
	buf += this_write;
	len -= this_write;
	actual += this_write;
	file->pos += this_write;
    }
end:
    if(file->pos > file->inode->inode.size)
    {
	file->inode->inode.size = file->pos;
	file->inode->dirty = TRUE;
    }
    if(actual > 0)
    {
	file->inode->inode.modtime = current_time();
	file->inode->dirty = TRUE;
    }
#if 0
    write_inode(file->inode);
#endif
    return actual;
}

static bool
delete_indirect_blocks(struct core_inode *inode, blkno blk, int depth)
{
    struct buf_head *ind_blk = bread(inode->dev, blk);
    bool rc = TRUE;
    int i;
    if(ind_blk == NULL)
	return FALSE;
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return FALSE;
    }
    for(i = 0; i < PTRS_PER_INDIRECT; i++)
    {
	if(ind_blk->buf.ind.data[i] != 0)
	{
	    if(depth == 0)
		free_block(inode->dev, ind_blk->buf.ind.data[i]);
	    else
	    {
		if(!delete_indirect_blocks(inode, ind_blk->buf.ind.data[i],
					   depth - 1))
		{
		    rc = FALSE;
		    break;
		}
	    }
	    ind_blk->buf.ind.data[i] = 0;
	    bdirty(ind_blk, FALSE);
	}
    }
    brelse(ind_blk);
    return rc;
}

/* Deletes all data blocks associated with INODE and sets it length to
   zero. */
bool
delete_inode_data(struct core_inode *inode)
{
    int i;
    bool rc = TRUE;
    if(!test_media(inode->dev))
	return FALSE;
    if(inode->invalid)
    {
	ERRNO = E_INVALID;
	return FALSE;
    }
    for(i = 0; i < SINGLE_INDIRECT; i++)
    {
	if(inode->inode.data[i] != 0)
	{
	    free_block(inode->dev, inode->inode.data[i]);
	    inode->inode.data[i] = 0;
	    inode->dirty = TRUE;
	}
    }
    if(inode->inode.data[SINGLE_INDIRECT] != 0)
    {
	rc = delete_indirect_blocks(inode, inode->inode.data[SINGLE_INDIRECT],
				    0);
	inode->inode.data[SINGLE_INDIRECT] = 0;
    }
    if(rc && inode->inode.data[DOUBLE_INDIRECT] != 0)
    {
	rc = delete_indirect_blocks(inode, inode->inode.data[DOUBLE_INDIRECT],
				    1);
	inode->inode.data[DOUBLE_INDIRECT] = 0;
    }
    if(rc && inode->inode.data[TRIPLE_INDIRECT] != 0)
    {
	rc = delete_indirect_blocks(inode, inode->inode.data[TRIPLE_INDIRECT],
				    2);
	inode->inode.data[TRIPLE_INDIRECT] = 0;
    }
    inode->inode.size = 0;
    inode->inode.modtime = current_time();
    inode->dirty = TRUE;
#if 0
    write_inode(inode);
#endif
    return rc;
}

/* Deletes all data blocks associated with FILE and sets its length to
   zero. */
bool
truncate_file(struct file *file)
{
    if(file == NULL)
    {
	ERRNO = E_BADARG;
	return FALSE;
    }
    return delete_inode_data(file->inode);
}

bool
set_file_size(struct file *file, size_t size)
{
    if(file == NULL)
    {
	ERRNO = E_BADARG;
	return FALSE;
    }
    F_SIZE(file) = size;
    file->inode->dirty = TRUE;
    write_inode(file->inode);
    return TRUE;
}

bool
set_file_modes(const char *name, u_long mode)
{
    struct file *file = open_file(name, F_READ);
    if(file != NULL)
    {
	file->inode->inode.attr &= ~ATTR_MODE_MASK;
	file->inode->inode.attr |= (mode & ~ATTR_MODE_MASK);
	file->inode->dirty = TRUE;
	close_file(file);
	return TRUE;
    }
    ERRNO = E_BADARG;
    return FALSE;
}
