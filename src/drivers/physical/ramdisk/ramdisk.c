/* ramdisk.c -- The ramdisk driver.
 * Simon Evans, C.Luke
 */

#include <vmm/types.h>
#include <vmm/kernel.h>
#include <vmm/ramdisk.h>
#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/lists.h>
#include <vmm/string.h>

#define BLKDEV_TYPE rd_dev_t
#define BLKDEV_NAME "rd"
#include <vmm/blkdev.h>

#define kprintf kernel->printf
#define REQ_RD_DEV(r) ((rd_dev_t *)(r->dev))

/* Device structures for the ramdisks. */
list_t rd_dev_list;


/* The list of FD requests, the first element is the request to process
   after the current one completes. Note that any access to this structure
   must only take place with interrupts masked. */
static list_t rd_reqs;

/* The request currently being processed, or NULL. Same warning applies
   about interrupts as above. */
static blkreq_t *current_req;

/* This is put into `current_req' when we're in the middle of doing
   something that doesn't have an rd_req_t. */
#define FAKE_REQ ((blkreq_t *)0xFD00FD00)


static void do_request(blkreq_t *req);

struct fs_module *fs;


/* Pull in the two functions from <vmm/blkdev.h> */

END_REQUEST_FUN(current_req)
SYNC_REQUEST_FUN(do_request)



/* If no request is currently being processed and there's new requests in
   the queue, process the first one. This can be called from an interrupt
   or the normal kernel context. */
static void
do_request(blkreq_t *req)
{
    rd_dev_t *dev;
    u_long flags;

    save_flags(flags);

    /* This label is used to eliminate tail-recursion. */
top:

    cli();
    if(current_req != NULL)
    {
	if(req != NULL)
	    append_node(&rd_reqs, &req->node);
	load_flags(flags);
	return;
    }
    if(req == NULL)
    {
	if(!list_empty_p(&rd_reqs))
	{
	    req = (blkreq_t *)rd_reqs.head;
	    remove_node(&req->node);
	}
	else
	{
	    load_flags(flags);
	    return;
	}
    }
    current_req = req;
    load_flags(flags);

    dev = REQ_RD_DEV(req);
    DB(("rd:do_request: req=%p drive=%d block=%d nblocks=%d cmd=%d buf=%p\n",
	req, dev->drvno, req->block, req->nblocks, req->command, req->buf));

    if(req->block >= dev->total_blocks)
    {
	kprintf("rd: Device %s (%p) doesn't have a block %d!\n",
		dev->name, dev, req->block);
	end_request(-1);
	req = NULL;
	goto top;
    }

    switch(req->command)
    {

    case RD_CMD_READ:	
	DB(("dev->ram = %p\n", dev->ram));
	memcpy(req->buf, dev->ram + (req->block * FS_BLKSIZ/2),
		req->nblocks * FS_BLKSIZ/2);
	end_request(0);
	req = NULL;
	goto top;


    case RD_CMD_WRITE:
	DB(("dev->ram = %p\n", dev->ram));
	memcpy(dev->ram + (req->block * FS_BLKSIZ/2), req->buf,
		req->nblocks * FS_BLKSIZ/2);
	end_request(0);
	req = NULL;
	goto top;

    default:
	kprintf("rd:do_request: Unknown command in rd_req, %d\n",
		req->command);
	end_request(-1);
	req = NULL;
	goto top;
    }
}


rd_dev_t *
create_ramdisk(u_long blocks)
{
	static int nextdrv = 0;
	rd_dev_t *new = kernel->malloc(sizeof(rd_dev_t));
	if(new == NULL)
		return NULL;
	new->ram = kernel->malloc(blocks * 512);
	if(new->ram == NULL) 
		return NULL;
	new->drvno = nextdrv;
	kernel->sprintf(new->name, "%s%d", BLKDEV_NAME, nextdrv++);
	new->total_blocks = blocks;
DB(("rd: making file system\n")); 
	if(ramdisk_mkfs_disk(new, 0) == FALSE) {
		DB(("\nmkfs failed!\n"));
		return FALSE;
	} else {
		DB(("\nmkfs worked ok!\n"));
	}
DB(("rd: appending node\n"));
	append_node(&rd_dev_list, &new->node);
DB(("done\n"));
	if(ramdisk_mount_disk(new) == FALSE)
		return NULL;
	return new;
}

bool
delete_ramdisk(rd_dev_t *rd)
{
    /* try to umount the device first */
    struct fs_device *dev;
    list_node_t *nxt, *x = rd_dev_list.head;
    rd_dev_t *lrd;

    dev = fs->get_device(rd->name);
    if(dev == NULL) return FALSE;
    if(!fs->remove_device(dev))
    {
        fs->release_device(dev);
	return FALSE;
    }
    fs->release_device(dev);

    while((nxt = x->succ) != NULL)
    {
        if(rd == (rd_dev_t *)x) {
            lrd = (rd_dev_t *)x;
            remove_node(x);
            kernel->free(lrd->ram);
            kernel->free(lrd);
            return TRUE;
        }
        x = nxt;
    }
    return FALSE;
}

bool
ramdisk_init(void)
{
	init_list(&rd_reqs);
	init_list(&rd_dev_list);
	fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
        if(create_ramdisk(1440) == NULL) return FALSE;
	DB(("ramdisk created\n"));
	add_ramdisk_commands();
	return TRUE;
}


/* Filesystem interface... All block based and such... */

long
ramdisk_read_blocks(rd_dev_t *rd, void *buf, u_long block, int count)
{
	blkreq_t req;
	req.buf = buf;
	req.command = RD_CMD_READ;
	req.block = block;
	req.nblocks = count;
	req.dev = (rd_dev_t *)rd;
	return sync_request(&req);
}


long
ramdisk_write_blocks(rd_dev_t *rd, void *buf, u_long block, int count)
{
	blkreq_t req;
	req.buf = buf;
	req.command = RD_CMD_WRITE;
	req.block = block;
	req.nblocks = count;
	req.dev = (rd_dev_t *)rd;
	return sync_request(&req);
}

long
ramdisk_fs_read_blocks(void *f, blkno block, void *buf, int count)
{
	return ramdisk_read_blocks(f, buf, block * (FS_BLKSIZ / 512),
				   count * (FS_BLKSIZ / 512)) ? 1 : E_IO;
}


long
ramdisk_fs_write_blocks(void *f, blkno block, void *buf, int count)
{
	return ramdisk_write_blocks(f, buf, block * (FS_BLKSIZ / 512),
				    count * (FS_BLKSIZ / 512)) ? 1 : E_IO;
}


bool
ramdisk_mount_disk(rd_dev_t *rd) 
{
	struct fs_device *dev;

	dev = fs->alloc_device();
	if(dev != NULL) {
		dev->name = rd->name;
		dev->read_blocks = ramdisk_fs_read_blocks;
		dev->write_blocks = ramdisk_fs_write_blocks;
		dev->test_media = NULL;
		dev->user_data = rd;
		if(fs->add_device(dev)) {
			kprintf("%s added\n", rd->name);
		} else {
			fs->remove_device(dev);
			return FALSE;
		}
	}
	return TRUE;
}


bool
ramdisk_add_dev(void)
{
	return FALSE;
}

bool
ramdisk_remove_dev(void)
{
	return FALSE;
}

bool
ramdisk_mkfs_disk(rd_dev_t *rd, u_long reserved)
{
    bool rc = FALSE;
    struct fs_device *dev = fs->get_device(rd->name);
    if(dev != NULL)
    {
        fs->release_device(dev);
        kprintf("rd: Can't mkfs `%s', it's mounted\n", rd->name);
        return FALSE;
    }
    dev = fs->alloc_device();
    if(dev != NULL)
    {
        dev->name = rd->name;
        dev->read_blocks = ramdisk_fs_read_blocks;
        dev->write_blocks = ramdisk_fs_write_blocks;
        dev->test_media = NULL;
        dev->user_data = rd;
        dev->read_only = FALSE;
        if(fs->mkfs(dev, rd->total_blocks / (FS_BLKSIZ / 512), reserved))
            rc = TRUE;
    }
    return rc;
}
