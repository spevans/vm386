/* dev.c -- device handling
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/io.h>

#ifndef TEST
#define kprintf kernel->printf
#endif

struct fs_device *device_list;

static struct fs_device device_pool[NR_DEVICES];
static struct fs_device *dev_free_list;

void
init_devices(void)
{
    int i;
    for(i = 0; i < NR_DEVICES-1; i++)
	device_pool[i].next = &device_pool[i+1];
    device_pool[i].next = NULL;
    dev_free_list = device_pool;
}

struct fs_device *
alloc_device(void)
{
    struct fs_device *dev;
    FORBID();
    dev = dev_free_list;
    if(dev != NULL)
	dev_free_list = dev->next;
    PERMIT();
    return dev;
}

static void
free_device(struct fs_device *dev)
{
    FORBID();
    dev->next = dev_free_list;
    dev_free_list = dev;
    PERMIT();
}

#define TO_UPPER(c) ((((c) >= 'a') && ((c) <= 'z')) ? ((c) + 0x20) : (c))

static inline int
devcmp(const u_char *p1, const u_char *p2)
{
    /* I borrowed this from the Linux C library :-) */
    register int ret;
    u_char c1;
    if(p1 == p2)
	return 0;
    for (; !(ret = (c1 = TO_UPPER(*p1)) - TO_UPPER(*p2)); p1++, p2++)
    {
	if (c1 == '\0')
	    break;
    }
    return ret;
}

/* Add a new device DEV to the system. */
bool
add_device(struct fs_device *dev)
{
    dev->root = NULL;
    dev->use_count = 1;
    dev->invalid = TRUE;
    FORBID();
    dev->next = device_list;
    device_list = dev;
    PERMIT();
    validate_device(dev);
    return TRUE;
}

/* Return the device called NAME, incrementing it's use_count before doing
   so. */
struct fs_device *
get_device(const char *name)
{
    struct fs_device *dev;
    FORBID();
    dev = device_list;
    while(dev != NULL)
    {
	if(!devcmp(name, dev->name))
	{
	    dev->use_count++;
	    break;
	}
	dev = dev->next;
    }
    PERMIT();
    return dev;
}

/* Release a pointer DEV to a device. If this is the last reference
   the device is freed. */
void
release_device(struct fs_device *dev)
{
    if(--dev->use_count <= 0)
    {
	/* Last one out turn off the light.. */
	invalidate_device(dev);
	kprintf("fs: Device `%s' has been discarded.\n", dev->name);
	free_device(dev);
    }
}

/* Remove the device DEV from the system. It's not actually freed until
   all pointers to it go as well. */
bool
remove_device(struct fs_device *dev)
{
    struct fs_device **devp;
#if 0
    if(dev->use_count > 1)
    {
	ERRNO = E_INUSE;
	return FALSE;
    }
#endif
    FORBID();
    devp = &device_list;
    while(*devp)
    {
	if(*devp == dev)
	{
	    *devp = dev->next;
	    dev->next = NULL;
	    break;
	}
	devp = &((*devp)->next);
    }
    PERMIT();
    release_device(dev);
    return TRUE;
}

/* Read the super-block information of DEV. */
bool
read_meta_data(struct fs_device *dev)
{
    static struct boot_blk tmp_bb;
    long result = FS_READ_BLOCKS(dev, BOOT_BLK, &tmp_bb, 1);
    if(result < 0)
    {
	ERRNO = -result;
	return FALSE;
    }
    if((tmp_bb.magic != FS_BOOT_MAGIC)
       && (tmp_bb.magic != FS_NOBOOT_MAGIC))
    {
	kprintf("fs: Warning bad magic number on device `%s'\n", dev->name);
	ERRNO = E_BADMAGIC;
	return FALSE;
    }
    memcpy(&dev->sup, &tmp_bb.sup, sizeof(struct super_data));
    dev->read_only = FALSE;	/* Fix this. */
    return TRUE;
}

/* Call this when the device becomes invalid. */
void
invalidate_device(struct fs_device *dev)
{
    if(!dev->invalid)
    {
        if(dev->root != NULL)
        {
	    dev->root->invalid = TRUE; /* Stop close_inode() writing it */
	    close_inode(dev->root);
        }
	dev->root = NULL;
	dev->invalid = TRUE;
	flush_device_cache(dev, TRUE);
	invalidate_device_inodes(dev);
    }
}

/* Call this to re-validate the device DEV. */
bool
validate_device(struct fs_device *dev)
{
    if(!dev->invalid)
	return TRUE;
    if(!read_meta_data(dev))
	return FALSE;
    dev->invalid = FALSE;
    dev->root = make_inode(dev, ROOT_INUM);
    if(dev->root == NULL)
	return FALSE;
    return TRUE;
}
