/* floppy.c -- The floppy driver.
 * Gutted and coded C.Luke
 */

#define DEBUG
#include "fd.h"

/* Allocated trackbuffer for DMA i/o */
char *track_buf;

/* DMA info structure thingy */
struct DmaBuf DMAbuf;

/* Device structures for the two disks. */
fd_dev_t fd_devs[2];

/* The function to dispatch the next IRQ to (or NULL). */
void (*fd_intr)(void);

/* The list of FD requests, the first element is the request to process
   after the current one completes. Note that any access to this structure
   must only take place with interrupts masked. */
list_t fd_reqs;

/* The request currently being processed, or NULL. Same warning applies
   about interrupts as above. */
blkreq_t *current_req;

/* This is whether we want the timer to keep running... */
bool int_timer_flag;
struct timer_req timer;


/* Pull in the two functions from <vmm/blkdev.h> */

END_REQUEST_FUN(current_req)
SYNC_REQUEST_FUN(do_request)


/* IRQ dispatcher. */
static void fd_int_handler(void)
{
    void (*func)(void) = fd_intr;
    DB(("fd_int_handler: fd_intr=%p\n", fd_intr));
    if(func != NULL)
    {
	fd_intr = NULL;
	func();
    }
    else
    {
	/* Unexpected interrupt. */
	kprintf("fd: Unexpected interrupt received.\n");
    }
#ifdef TEST
    outb_p(0x20, 0xA0);
    outb_p(0x20, 0x20);
#endif
}

/* If no request is currently being processed and there's new requests in
   the queue, process the first one. This can be called from an interrupt
   or the normal kernel context. */
void do_request(blkreq_t *req)
{
    fd_dev_t *dev;
    u_long track, sect, cyl, head, big_sect, sects;
    u_long flags;
    int i;

    save_flags(flags);

    /* This label is used to eliminate tail-recursion. */
top:

    cli();
    if(current_req != NULL)
    {
	if(req != NULL)
	    append_node(&fd_reqs, &req->node);
	load_flags(flags);
	return;
    }
    for(i = 0; i < 2; i++)
    {
	if(fd_devs[i].recalibrate)
	{
	    fdc_recal(&fd_devs[i]);
	    if(req != NULL)
		append_node(&fd_reqs, &req->node);
	    load_flags(flags);
	    return;
	}
    }
    if(req == NULL)
    {
	if(!list_empty_p(&fd_reqs))
	{
	    req = (blkreq_t *)fd_reqs.head;
	    remove_node(&req->node);
	}
	else
	{
	    load_flags(flags);
	    return;
	}
    }
    current_req = req;
#if 0
    req->retries = 0;
#endif
    load_flags(flags);

    dev = REQ_FD_DEV(req);

    DB(("fd:do_request: req=%p drive=%d block=%d nblocks=%d cmd=%d buf=%p\n",
	req, dev->drvno, req->block, req->nblocks, req->command, req->buf));

    switch(req->command)
    {
    case FD_CMD_SEEK:	/* We wanna MOVE DA HEAD! */
        /* Do da seek. */
	if(fdc_seek(dev, req->block) == FALSE)
	{
		handle_error("FD_CMD_SEEK, seek");
		goto top;
		break;
	}

	/* Then Sense Interrupt Status */
	if(fdc_sense() == FALSE)
	{
		handle_error("FD_CMD_SEEK, fdc_sense");
		goto top;
		break;
	}

	/* and now we have to Read the ID */
	if(fdc_read_id(dev) == FALSE)
	{
		handle_error("FD_CMD_SEEK, read_id");
		goto top;
		break;
	}

        fd_end_request(0);
	req = NULL;
        goto top;

    case FD_CMD_TIMER:
	fd_end_request(0);
	req = NULL;
	goto top;
    }

    if(req->block >= dev->total_blocks)
    {
	kprintf("fd: Device %s (%p) doesn't have a block %d!\n",
		dev->name, dev, req->block);
	fd_end_request(-1);
	req = NULL;
	goto top;
    }

    big_sect = req->block;
    sects = req->nblocks;

    track = big_sect / dev->disk_p->sectors;
    sect = big_sect % dev->disk_p->sectors + 1;
    head = track % dev->disk_p->heads;
    cyl = track / dev->disk_p->heads;

    DB(("fd:do_request: cyl=%d sect=%d head=%d\n", cyl, sect, head));

    switch(req->command)
    {
    case FD_CMD_READ:	/* We wanna READ the floppy! */

#if 0
        fd_end_request(0);
	req = NULL;
        goto top;
#endif

	/* We need to seek to the right cylinder. */
	if(fdc_seek(dev, cyl) == FALSE)
	{
		handle_error("FD_CMD_READ, seek");
		goto top;
		break;
	}

	/* Then Sense Interrupt Status */
	if(fdc_sense() == FALSE)
	{
		handle_error("FD_CMD_READ, fdc_sense");
		goto top;
		break;
	}

	/* and now we have to Read the ID */
	if(fdc_read_id(dev) == FALSE)
	{
		handle_error("FD_CMD_READ, read_id");
		goto top;
		break;
	}

#define TPA(XX) ((u_long)TO_PHYSICAL(XX))

	/* Tell the DMA what to do, and hope for the best! */
	/* Should move this inside fdc, in fdc_read() i think */
	DMAbuf.Buffer = track_buf;
	DMAbuf.Page = (u_int8)((TPA(track_buf) >> 16) & 0xff);
	DMAbuf.Offset = (u_int16)(TPA(track_buf) & 0xffff);
	DMAbuf.Len = (u_int16)(dev->disk_p->sectors * dev->disk_p->heads *
		FD_SECTSIZ) - 1;
	DMAbuf.Chan = FLOPPY_DMA;
	kernel->setup_dma(&DMAbuf, DMA_READ);

	/* Now we issue a read command. */
	if(fdc_read(dev, cyl) == FALSE)
	{
		handle_error("FD_CMD_READ, read");
		goto top;
		break;
	}

	break;

    case FD_CMD_WRITE:	/* We wanna WRITE it too! */

        fd_end_request(0);
	req = NULL;
        goto top;

    default:
	kprintf("fd:do_request: Unknown command in fd_req, %d\n",
		req->command);
	fd_end_request(-1);
	req = NULL;
	goto top;
    }
}

int fd_sync_request(blkreq_t *req)
{
	return sync_request(req);
}

void fd_end_request(int i)
{
	end_request(i);
}

/* Clean up after an error. The caller should usually call do_request()
   after this function returns. It can be called from an IRQ handler or the
   normal kernel context. */
void handle_error(const char *from)
{
    u_long flags;
    if(current_req == NULL)
	return;
    save_flags(flags);
    cli();
    kprintf("\nfd: %s (%s): Error (retry number %d)\n",
        from, REQ_FD_DEV(current_req)->name, current_req->retries);
    dump_stat();
    fd_intr = NULL;
    if(current_req->retries++ < MAX_RETRIES)
    {
#if 0
	if((current_req->retries % RESET_FREQ) == 0)
	    reset_pending = TRUE;
#endif
	if((current_req->retries % RECAL_FREQ) == 0)
	    REQ_FD_DEV(current_req)->recalibrate = TRUE;
	/* Retry the current request, this simply means stacking it on the
	   front of the queue and calling do_request(). */
	prepend_node(&fd_reqs, &current_req->node);
	current_req = NULL;
	DB(("fd:handle_error: Retrying request %p\n", current_req));
    }
    else
    {
#if 0
	reset_pending = TRUE;
#endif
	REQ_FD_DEV(current_req)->recalibrate = TRUE;
	DB(("\nfd: handle_error: Request %p has no more retries available.\n",
	    current_req));
	fd_end_request(-1);
    }
    load_flags(flags);
}


bool floppy_init(void)
{
	int drive_cnt = 0;
	int i;

	kprintf("fd: init\n");

	init_list(&fd_reqs);

	/* get the IRQ and DMA hardware and a DMA buffer */
	if(!kernel->alloc_irq(FLOPPY_IRQ, fd_int_handler, "fd")) {
		kprintf("fd: couldnt get IRQ %d\n", FLOPPY_IRQ);
		return FALSE;
	}
	if(reset_fdc() == FALSE) {
		kprintf("fd: reset_fdc failed\n");
		kernel->dealloc_irq(FLOPPY_IRQ);
		return FALSE;
	}
	if(!kernel->alloc_dmachan(FLOPPY_DMA, "fd")) {
		kprintf("fd: couldn't get DMA channel %d\n", FLOPPY_DMA);
		kernel->dealloc_irq(FLOPPY_IRQ);
		return FALSE;
	}
	if((track_buf = (char *)kernel->alloc_pages_64(TRACKBUF_PAGES)) == NULL) {
		kprintf("fd: couldn't allocate DMA buffer\n");
		kernel->dealloc_irq(FLOPPY_IRQ);
		kernel->dealloc_dmachan(FLOPPY_DMA);
		return FALSE;
	}
	DB(("DMA BUFFER: track_buf: %08p\n", track_buf));
	for(i = 0; i < (TRACKBUF_PAGES * 4096); i++)
		track_buf[i] = i%256;

	/* Start our three-second timer... */
	int_timer_flag = TRUE;
	set_timer_func(&timer, FD_TIMEINT, timer_intr, NULL);
	kernel->add_timer(&timer);

	/* What drives have we got? */
	for(i = 0; i < 2; i++) {
		if(kernel->cookie->floppy_types[i]) {
			/* now fill in the device structure */
			fd_devs[i].name = (i == 0) ? "fd0" : "fd1";
			fd_devs[i].drvno = i;
			fd_devs[i].drive_p = &main_drive_params[kernel->cookie->floppy_types[i]];
			fd_devs[i].disk_p = &main_disk_params[fd_devs[i].drive_p->native_format];
			fd_devs[i].total_blocks = fd_devs[i].disk_p->blocks;

			fdc_recal(&fd_devs[i]);

			kprintf("fd%d: %s ", i, fd_devs[i].drive_p->name);
			drive_cnt++;
#if 0
			floppy_add_dev(&fd_devs[i]);
#endif
		}
	}
	if(!drive_cnt) {
		kprintf("fd: No drives found...\n");
		kernel->dealloc_irq(FLOPPY_IRQ);
		kernel->dealloc_dmachan(FLOPPY_DMA);
		kernel->free_pages((page *)track_buf, TRACKBUF_PAGES);
		int_timer_flag = FALSE;
		return FALSE;
	}
			 
	kprintf("fd: init okay\n");
	return TRUE;
}
