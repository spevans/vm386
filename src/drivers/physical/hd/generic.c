/* generic.c -- Generic hard disk handling.
   John Harper. */

/* This is getting very general; so much so that it might be better to
   change this into generic *block* device support, not generic hard-disk
   device support? */

#ifdef TEST
# ifndef GO32
#  error Need go32
# endif
# include <test/go32prt.h>
#endif

#include <vmm/types.h>
#include <vmm/string.h>
#include <vmm/hd.h>
#include <vmm/fs.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/errno.h>

#ifndef TEST
# define kprintf kernel->printf
# define ksprintf kernel->sprintf
#endif


/* Hard disk device structure management. */

static struct fs_module *fs;

/* List of all hard-disk like devices in the system. */
hd_dev_t *hd_list;

static inline void
add_hd(hd_dev_t *hd)
{
    cli();
    hd->next = hd_list;
    hd_list = hd;
    sti();
}


/* Partition structure management. */

/* The maximum number of partitions in the system. */
#define NR_PARTITIONS 32

static hd_partition_t partition_pool[NR_PARTITIONS];
static hd_partition_t *free_partitions;
hd_partition_t *partition_list;

void
init_partitions(struct fs_module *_fs)
{
    int i;
    fs = _fs;
    for(i = 0; i < (NR_PARTITIONS-1); i++)
	partition_pool[i].next = &partition_pool[i+1];
    partition_pool[i].next = NULL;
    free_partitions = partition_pool;
    partition_list = NULL;
}

static inline hd_partition_t *
alloc_partition(void)
{
    hd_partition_t *new;
    forbid();
    new = free_partitions;
    if(new != NULL)
    {
	free_partitions = new->next;
	new->next = NULL;
    }
    permit();
    return new;
}

static inline void
free_partition(hd_partition_t *p)
{
    forbid();
    p->next = free_partitions;
    free_partitions = p;
    permit();
}

static inline void
add_partition(hd_partition_t *p)
{
    forbid();
    p->next = partition_list;
    partition_list = p;
    permit();
}

static inline void
remove_partition(hd_partition_t *p)
{
    hd_partition_t **x;
    forbid();
    x = &partition_list;
    while(*x != NULL)
    {
	if(*x == p)
	{
	    *x = p->next;
	    break;
	}
	x = &(*x)->next;
    }
    permit();
}

hd_partition_t *
hd_find_partition(const char *name)
{
    hd_partition_t *p;
    forbid();
    p = partition_list;
    while(p != NULL)
    {
	if(strcmp(p->name, name) == 0)
	    break;
	p = p->next;
    }
    permit();
    return p;
}


/* Code to translate the partition table and any extended partitions
   into a list of logical partitions and generally initialise a generic
   hard disk device. */

/* This structure defines the layout of one partition table entry in
   a disks mbr, or in a link in an extended partition. Note that the
   `start_sector' and `end_sector' fields also contain the two high
   bits of the related cylinder values. */
struct msdos_partition {
    u_char boot;
    u_char start_head, start_sector, start_cyl;
    u_char system;
    u_char end_head, end_sector, end_cyl;
    u_long start_block;
    u_long total_blocks;
};

#define TEST_SIG(blk) (*(u_short *)(((char *)(blk)) + 510) == 0xAA55)


static hd_partition_t *
new_partition(struct msdos_partition *p, int minor, hd_dev_t *hd,
	      u_long start, u_long size)
{
    hd_partition_t *par;
    par = alloc_partition();
    if(par != NULL)
    {
	par->start = start;
	par->size = size;
	par->hd = hd;
	ksprintf(par->name, "%s%d", hd->name, minor);
	kprintf(" %s", par->name);
	add_partition(par);
	if(p->system == VMM_PARTITION_TYPE)
	{
	    if(hd_mount_partition(par, FALSE))
		kprintf(":");
	}
    }
    return par;
}

/* Initialise the hard-disk-like device HD. This involves creating a
   partition to represent the whole device and one for each actual
   partition on the device.  */
bool
hd_add_dev(hd_dev_t *hd)
{
    hd_partition_t *par;
    u_char mbr[512];

    if(strlen(hd->name) > PARTN_NAME_MAX-2)
	return FALSE;

    /* Make a partition spanning the whole device with the same name as the
       device. */
    par = alloc_partition();
    if(par == NULL)
	return FALSE;
    par->start = 0;
    par->size = hd->total_blocks;
    par->hd = hd;
    strcpy(par->name, hd->name);
    add_partition(par);
    kprintf("%s:", par->name);

    if(hd->read_blocks(hd, mbr, 0, 1)
       && TEST_SIG(mbr))
    {
	int pri, ext;
	struct msdos_partition *p = (struct msdos_partition *)(mbr + PART_TABLE_OFFSET);
	for(pri = 1, ext = 5; pri < 5; pri++, p++)
	{
	    if(p->system == 0 || p->total_blocks == 0)
		continue;
	    new_partition(p, pri, hd, p->start_block, p->total_blocks);
	    if(p->system == 5)
	    {
		/* Extended partition. */
		u_long ext_start = p->start_block;
		u_long curr_start = ext_start;
		kprintf(" <");
		while(1)
		{
		    u_char embr[512];
		    struct msdos_partition *p = (struct msdos_partition *)(embr + PART_TABLE_OFFSET);
		    if(!hd->read_blocks(hd, embr, curr_start, 1)
		       || !TEST_SIG(embr))
		    {
			break;
		    }
		    /* First entry is the actual data partition... */
		    new_partition(p, ext, hd, p->start_block + curr_start,
				  p->total_blocks);
		    ext++;
		    p++;
		    /* ...second is a link to the next (extended) partition
		       table. */
		    if(p->system != 5)
			break;
		    curr_start = ext_start + p->start_block;
		}
		kprintf(" >");
	    }
	}
    }
    kprintf("\n");
    add_hd(hd);
    return TRUE;
}

// FIXME
/* Remove the hard-disk-like device DEV from the system. Returns TRUE if
   it succeeds, FALSE otherwise. */
bool
hd_remove_dev(hd_dev_t *dev)
{
    /* This needs some thought. A device can only be removed if no one
       is accessing it. */
    (void)dev; // unused for now
    return FALSE;
}


/* Generic functions to read and write blocks on any partition. Both check
   the validity of their BLOCK arguments. */

inline bool
hd_read_blocks(hd_partition_t *p, void *buf, u_long block, int count)
{
    if((block + count) > p->size)
	return FALSE;
    return p->hd->read_blocks(p->hd, buf, block + p->start, count);
}

inline bool
hd_write_blocks(hd_partition_t *p, void *buf, u_long block, int count)
{
    if((block + count) > p->size)
	return FALSE;
    return p->hd->write_blocks(p->hd, buf, block + p->start, count);
}


/* Functions to interface a logical partition to the file system. */

static long
hd_fs_read_blocks(void *p, blkno block, void *buf, int count)
{
    return hd_read_blocks(p, buf, block * (FS_BLKSIZ / 512),
			  count * (FS_BLKSIZ / 512)) ? 1 : -E_IO;
}

static long
hd_fs_write_blocks(void *p, blkno block, void *buf, int count)
{
    return hd_write_blocks(p, buf, block * (FS_BLKSIZ / 512),
			   count * (FS_BLKSIZ / 512)) ? 1 : -E_IO;
}

/* Mount the logical partition P in the file system. If READ-ONLY is TRUE
   no modifications will be allowed to P. */
bool
hd_mount_partition(hd_partition_t *p, bool read_only)
{
    bool rc = FALSE;
    struct fs_device *dev;
    dev = fs->get_device(p->name);
    if(dev != NULL)
    {
	fs->release_device(dev);
	kprintf("hd: Partition `%s' is already mounted\n", p->name);
	return FALSE;
    }
    dev = fs->alloc_device();
    if(dev != NULL)
    {
	dev->name = p->name;
	dev->read_blocks = hd_fs_read_blocks;
	dev->write_blocks = hd_fs_write_blocks;
	dev->test_media = NULL;
	dev->user_data = p;
	dev->read_only = read_only;	/* ? */
	if(fs->add_device(dev))
	{
	    DB(("total_blocks=%d inode_bitmap=%d inodes=%d num_inodes=%d\n"
		"data_bitmap=%d data=%d data_size=%d\n",
		dev->sup.total_blocks, dev->sup.inode_bitmap,
		dev->sup.inodes, dev->sup.num_inodes, dev->sup.data_bitmap,
		dev->sup.data, dev->sup.data_size));
	    rc = TRUE;
	}
	else
	    fs->remove_device(dev);
    }
    return rc;
}

/* Create a file system on the unmounted partition P. */
bool
hd_mkfs_partition(hd_partition_t *p, u_long reserved)
{
    bool rc = FALSE;
    struct fs_device *dev = fs->get_device(p->name);
    if(dev != NULL)
    {
	fs->release_device(dev);
	kprintf("hd: Can't mkfs `%s', it's mounted\n", p->name);
	return FALSE;
    }
    dev = fs->alloc_device();
    if(dev != NULL)
    {
	dev->name = p->name;
	dev->read_blocks = hd_fs_read_blocks;
	dev->write_blocks = hd_fs_write_blocks;
	dev->test_media = NULL;
	dev->user_data = p;
	dev->read_only = FALSE;
	if(fs->mkfs(dev, p->size / (FS_BLKSIZ / 512), reserved))
	    rc = TRUE;
    }
    return rc;
}
