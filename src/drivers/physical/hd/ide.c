/* ide.c -- IDE hard disk driver.
   John Harper. */

/* Define READ_ONLY if you want writes to be disallowed. */
#undef READ_ONLY

#include <vmm/types.h>
#include <vmm/io.h>
#include <vmm/lists.h>
#include <vmm/hd.h>
#include <vmm/hdreg.h>
#include <vmm/string.h>
#include <vmm/cookie_jar.h>
#include <vmm/tasks.h>
#include <vmm/kernel.h>
#include <vmm/irq.h>

#define kprintf kernel->printf
#define ksprintf kernel->sprintf

#define MAX_RETRIES	16
#define RESET_FREQ	8
#define RECAL_FREQ	4

#define IDE0_IRQ	14

#define IN_SECT(buf)	insw(buf, 256, HD_DATA)
#define OUT_SECT(buf)	outsw(buf, 256, HD_DATA)

#define GET_STAT()	inb_p(HD_STATUS)
#define GET_ERR()	inb_p(HD_ERROR)

#define BAD_RW_STAT	(HD_STAT_BSY | HD_STAT_ERR | HD_STAT_DWF)
#define DRV_RDY_STAT	(HD_STAT_DRDY | HD_STAT_DSC)
#define DATA_RDY_STAT	(DRV_RDY_STAT | HD_STAT_DRQ)

typedef struct {
    hd_dev_t hd;
    u_long precomp, lzone;		/* Unused by the driver */
    u_char ctl, select;
    u_char drvno;
    bool recalibrate;			/* TRUE when device should recal. */
} ide_dev_t;

#define BLKDEV_TYPE ide_dev_t
#define BLKDEV_NAME "ide"
#include <vmm/blkdev.h>



/* Device structures for the two disks on the primary IDE interface. */
static ide_dev_t ide_devs[2];

/* The function to dispatch the next IRQ to (or NULL). */
static void (*ide_intr)(void);

/* The list of IDE requests, the first element is the request to process
   after the current one completes. Note that any access to this structure
   must only take place with interrupts masked. */
static list_t ide_reqs;

/* The request currently being processed, or NULL. Same warning applies
   about interrupts as above. */
static blkreq_t *current_req;

/* This is put into `current_req' when we're in the middle of doing
   something that doesn't have an ide_req_t. */
#define FAKE_REQ ((blkreq_t *)0x1DE01DE0)

/* TRUE when the controller should be reset before the next request. */
static bool reset_pending;

static void do_request(blkreq_t *req);
static void handle_error(const char *from);


/* Pull in the two functions from <vmm/blkdev.h> */

END_REQUEST_FUN(current_req)
SYNC_REQUEST_FUN(do_request)



/* Returns TRUE iff all bits set in GOOD are set in STAT and none of the bits
   set in BAD are set in STAT. */
static inline bool
test_stat(u_char stat, int good, int bad)
{
    return (stat & (good | bad)) == good;
}

/* Print a description of the disk's status and error flags to the console. */
static void
dump_stat(void)
{
    u_char stat = GET_STAT();
    kprintf("ide: Status=%#2x [ ", stat);
    if(stat & HD_STAT_ERR)
	kprintf("Error ");
    if(stat & HD_STAT_IDX)
	kprintf("Index ");
    if(stat & HD_STAT_CORR)
	kprintf("CorrectedData ");
    if(stat & HD_STAT_DRQ)
	kprintf("DataRequest ");
    if(stat & HD_STAT_DSC)
	kprintf("DriveSeekComplete ");
    if(stat & HD_STAT_DWF)
	kprintf("DriveWriteFault ");
    if(stat & HD_STAT_DRDY)
	kprintf("DriveReady ");
    if(stat & HD_STAT_BSY)
	kprintf("Busy ");
    kprintf("]\n");
    if(stat & HD_STAT_ERR)
    {
	u_char err = GET_ERR();
	kprintf("ide: Error=%#2x [ ", err);
	if(err & HD_ERR_AMNF)
	    kprintf("AddressMarkNotFound ");
	if(err & HD_ERR_TK0NF)
	    kprintf("TrackZeroNotFound ");
	if(err & HD_ERR_ABRT)
	    kprintf("AbortedCommand ");
	if(err & HD_ERR_MCR)
	    kprintf("MediaChangeRequested ");
	if(err & HD_ERR_IDNF)
	    kprintf("IDNotFound ");
	if(err & HD_ERR_MC)
	    kprintf("MediaChanged ");
	if(err & HD_ERR_UNK)
	    kprintf("UncorrectableDataError ");
	if(err & HD_ERR_BBK)
	    kprintf("BadBlockDetected ");
	kprintf("]\n");
    }
}

/* Wait for the status flag to show non-busy, then do
   test_stat(stat, GOOD-STAT, BAD-STAT). If it fails, and FROM is non-NULL,
   call handle_error(FROM) then do_request(), else just return FALSE. */
static bool
wait_stat(int good_stat, int bad_stat, int retries, const char *from)
{
    while(GET_STAT() & HD_STAT_BSY)
    {
	if(--retries == 0)
	    goto error;
    }
    if(!test_stat(GET_STAT(), good_stat, bad_stat))
    {
    error:
	if(from != NULL)
	{
	    handle_error(from);
	    do_request(NULL);
	}
	return FALSE;
    }
    return TRUE;
}

/* IRQ handler for read requests. */
static void
read_intr(void)
{
    DB(("ide:read_intr: current_req=%p\n", current_req));
    if(current_req != NULL)
    {
	blkreq_t *req = current_req;
	if(!test_stat(GET_STAT(), DATA_RDY_STAT, BAD_RW_STAT))
	{
	    handle_error("read_intr");
	    do_request(NULL);
	    return;
	}
	IN_SECT(req->buf);
	DB(("ide:read_intr: Read sector, drive=%d block=%d\n",
	    req->dev->drvno, req->block));
	if(req->nblocks > 1)
	{
	    req->block++;
	    req->nblocks--;
	    req->buf += 512;
	    req->retries = 0;
	    ide_intr = read_intr;
	}
	else
	{
	    end_request(0);
	    do_request(NULL);
	}
    }
}

/* IRQ handler for write requests. */
static void
write_intr(void)
{
    DB(("ide:write_intr: current_req=%p\n", current_req));
    if(current_req != NULL)
    {
	blkreq_t *req = current_req;
	if(!test_stat(GET_STAT(), DRV_RDY_STAT, BAD_RW_STAT))
	{
	    handle_error("write_intr");
	    do_request(NULL);
	    return;
	}
	DB(("ide:write_intr: Done writing sector, drive=%d block=%d\n",
	    req->dev->drvno, req->block));
	if(req->nblocks > 1)
	{
	    req->block++;
	    req->nblocks--;
	    req->buf += 512;
	    req->retries = 0;
	    if(wait_stat(HD_STAT_DRQ, BAD_RW_STAT, 100000, "write_intr:DRQ"))
	    {
		OUT_SECT(req->buf);
		DB(("ide:write_intr: Output sector, drive=%d block=%d.\n",
		    req->dev->drvno, req->block));
		ide_intr = write_intr;
	    }
	}
	else
	{
	    end_request(0);
	    do_request(NULL);
	}
    }
}

/* IRQ dispatcher. */
static void
ide_int_handler(void)
{
    void (*func)(void) = ide_intr;
    DB(("ide:ide_int_handler: ide_intr=%p\n", ide_intr));
    if(func != NULL)
    {
	ide_intr = NULL;
	func();
    }
    else
    {
	/* Unexpected interrupt. */
	kprintf("ide: Unexpected interrupt received.\n");
    }
}

/* IRQ handler for recalibration requests. */
static void
recal_intr(void)
{
    if(test_stat(GET_STAT(), DRV_RDY_STAT, BAD_RW_STAT))
	kprintf("ok.\n");
    else
	kprintf("status error:\n");
    current_req = NULL;
    dump_stat();
    do_request(NULL);
}

/* Recalibrate device DEV. This returns before the recalibration
   has finished. */
static void
recalibrate_drive(ide_dev_t *dev)
{
    kprintf("ide: Recalibrating device `%s'.. ", dev->hd.name);
    outb_p(dev->select, HD_CURRENT);
    if(wait_stat(HD_STAT_DRDY, HD_STAT_BSY | HD_STAT_DRQ, 100000,
		 "recalibrate_drive:select"))
    {
	outb_p(dev->ctl, HD_DEVCTRL);
	ide_intr = recal_intr;
	dev->recalibrate = FALSE;
	current_req = FAKE_REQ;
	outb_p(HD_CMD_RECAL, HD_COMMAND);
    }
}

/* Soft-reset the IDE controller. */
static void
reset_controller(void)
{
    int i;
    reset_pending = FALSE;
    kprintf("ide: Resetting controller.. ");
    current_req = FAKE_REQ;
    outb_p(HD_SRST, HD_DEVCTRL);
    for(i = 0; i < 1000; i++)
	nop();
    outb_p(ide_devs[0].ctl, HD_DEVCTRL);
    for(i = 0; i < 1000; i++)
	nop();
    if(!wait_stat(DRV_RDY_STAT, HD_STAT_BSY, 500000, NULL))
    {
	kprintf("timed out, ");
	dump_stat();
    }
    if((i = GET_ERR()) == 1)
	kprintf("ok.\n");
    else
    {
	switch(i & 0x7f)
	{
	case 1:
	    kprintf("passed");
	    break;
	case 2:
	    kprintf("formatter device error");
	    break;
	case 3:
	    kprintf("sector buffer error");
	    break;
	case 4:
	    kprintf("ECC circuitry error");
	    break;
	case 5:
	    kprintf("controlling MPU error");
	    break;
	default:
	    kprintf("error #%d", i);
	}
	if(i & 0x80)
	    kprintf("; %s: error", ide_devs[1].hd.name);
	kprintf("\n");
    }
    current_req = NULL;
}
    
/* If no request is currently being processed and there's new requests in
   the queue, process the first one. This can be called from an interrupt
   or the normal kernel context. REQ is either a request to try and process
   or NULL. It will get added to the list of requests if necessary. */
static void
do_request(blkreq_t *req)
{
    ide_dev_t *dev;
    u_long track, sect, cyl, head;
    u_long flags;
    int i;

    save_flags(flags);

    /* This label is used to eliminate tail-recursion. */
top:

    cli();
    if(current_req != NULL)
    {
	if(req != NULL)
	    append_node(&ide_reqs, &req->node);
	load_flags(flags);
	return;
    }
    if(reset_pending)
	reset_controller();
    for(i = 0; i < 2; i++)
    {
	if(ide_devs[i].recalibrate)
	{
	    recalibrate_drive(&ide_devs[i]);
	    if(req != NULL)
		append_node(&ide_reqs, &req->node);
	    load_flags(flags);
	    return;
	}
    }
    if(req == NULL)
    {
	if(!list_empty_p(&ide_reqs))
	{
	    req = (blkreq_t *)ide_reqs.head;
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

    dev = req->dev;
    DB(("ide:do_request: req=%p drive=%d block=%d nblocks=%d cmd=%d buf=%p\n",
	req, dev->drvno, req->block, req->nblocks, req->command, req->buf));
    if(req->block >= dev->hd.total_blocks)
    {
	kprintf("ide: Device %s (%p) doesn't have a block %d!\n",
		dev->hd.name, dev, req->block);
	end_request(-1);
	req = NULL;
	goto top;
    }
    track = req->block / dev->hd.sectors;
    sect = req->block % dev->hd.sectors + 1;
    head = track % dev->hd.heads;
    cyl = track / dev->hd.heads;
    DB(("ide:do_request: cyl=%d sect=%d head=%d\n", cyl, sect, head));

    outb_p(dev->select, HD_CURRENT);
    if(wait_stat(HD_STAT_DRDY, HD_STAT_BSY | HD_STAT_DRQ, 100000,
		 "do_request:select"))
    {
	outb_p(dev->ctl, HD_DEVCTRL);
	outb_p(req->nblocks, HD_NSECTOR);
	outb_p(sect, HD_SECTOR);
	outb_p(cyl, HD_LCYL);
	outb_p(cyl >> 8, HD_HCYL);
	outb_p(dev->select | head, HD_CURRENT);

	switch(req->command)
	{
	case HD_CMD_READ:
	    ide_intr = read_intr;
	    outb_p(HD_CMD_READ, HD_COMMAND);
	    break;

	case HD_CMD_WRITE:
#ifndef READ_ONLY
	    ide_intr = write_intr;
	    outb_p(HD_CMD_WRITE, HD_COMMAND);
	    if(wait_stat(HD_STAT_DRQ, BAD_RW_STAT, 100000, "do_request:DRQ"))
	    {
		OUT_SECT(req->buf);
		DB(("ide:do_request: Output sector, drive=%d block=%d.\n",
		    req->dev->drvno, req->block));
	    }
#else
	    kprintf("ide:do_request: Write req when READ_ONLY defined\n");
	    end_request(-1);
	    req = NULL;
	    goto top;
#endif
	    break;

	default:
	    kprintf("ide:do_request: Unknown command in ide_req, %d\n",
		    req->command);
	    end_request(-1);
	    req = NULL;
	    goto top;
	}
    }
}

/* Clean up after an error. The caller should usually call do_request()
   after this function returns. It can be called from an IRQ handler or the
   normal kernel context. */
void
handle_error(const char *from)
{
    u_long flags;
    if(current_req == NULL)
	return;
    save_flags(flags);
    cli();
    kprintf("ide: %s (%s): Error\n", from, current_req->dev->hd.name);
    dump_stat();
    ide_intr = NULL;
    if(current_req->retries++ < MAX_RETRIES)
    {
	if((current_req->retries % RESET_FREQ) == 0)
	    reset_pending = TRUE;
	if((current_req->retries % RECAL_FREQ) == 0)
	    current_req->dev->recalibrate = TRUE;
	/* Retry the current request, this simply means stacking it on the
	   front of the queue and calling do_request(). */
	prepend_node(&ide_reqs, &current_req->node);
	current_req = NULL;
	DB(("ide:handle_error: Retrying request %p\n", current_req));
    }
    else
    {
	reset_pending = TRUE;
	current_req->dev->recalibrate = TRUE;
	DB(("ide:handle_error: Request %p has no more retries available.\n",
	    current_req));
	end_request(-1);
    }
    load_flags(flags);
}

/* Read COUNT blocks from the block BLOCK from the device HD to the
   buffer BUF. */
bool
ide_read_blocks(hd_dev_t *hd, void *buf, u_long block, int count)
{
    blkreq_t req;
    req.buf = buf;
    req.command = HD_CMD_READ;
    req.block = block;
    req.nblocks = count;
    req.dev = (ide_dev_t *)hd;
    return sync_request(&req);
}

/* Write COUNT blocks from the block BLOCK from the device HD to the
   buffer BUF. */
bool
ide_write_blocks(hd_dev_t *hd, void *buf, u_long block, int count)
{
    blkreq_t req;
    req.buf = buf;
    req.command = HD_CMD_WRITE;
    req.block = block;
    req.nblocks = count;
    req.dev = (ide_dev_t *)hd;
    return sync_request(&req);
}

/* Initialise everything. */
void
ide_init(void)
{
    int i;
    init_list(&ide_reqs);
    if(!kernel->alloc_irq(IDE0_IRQ, ide_int_handler, "hard disk"))
	return;
    /* Have to set up the device tables. */
    for(i = 0; i < 2; i++)
    {
	/* First test that the device exists. I do this by attempting to
	   select it then timing out on the status register. */
	outb_p(0xA0 | (i << 4), HD_CURRENT);
	if(wait_stat(HD_STAT_DRDY, HD_STAT_BSY, 100000, NULL))
	{
	    /* OK, it exists (I think..)
	       Next copy the bios drive info to the dev struct. */
	    ide_devs[i].hd.heads = kernel->cookie->hdisk[i].heads;
	    ide_devs[i].hd.cylinders = kernel->cookie->hdisk[i].cylinders;
	    ide_devs[i].hd.sectors = kernel->cookie->hdisk[i].sectors;
	    ide_devs[i].precomp = kernel->cookie->hdisk[i].precomp;
	    ide_devs[i].lzone = kernel->cookie->hdisk[i].lz;
	    ide_devs[i].ctl = kernel->cookie->hdisk[i].drive_opts > 8 ? 8 : 0;

	    ide_devs[i].hd.name = (i == 0) ? "hda" : "hdb";
	    ide_devs[i].hd.read_blocks = ide_read_blocks;
	    ide_devs[i].hd.write_blocks = ide_write_blocks;

	    ide_devs[i].select = 0xA0 | (i << 4);
	    ide_devs[i].hd.total_blocks = (ide_devs[i].hd.heads
					   * ide_devs[i].hd.sectors
					   * ide_devs[i].hd.cylinders);
	    ide_devs[i].drvno = i;

	    kprintf("%s: heads=%d cylinders=%d sectors=%d total_blocks=%d\n",
		    ide_devs[i].hd.name, ide_devs[i].hd.heads,
		    ide_devs[i].hd.cylinders, ide_devs[i].hd.sectors,
		    ide_devs[i].hd.total_blocks);

	    hd_add_dev(&ide_devs[i].hd);
	}
    }
}
