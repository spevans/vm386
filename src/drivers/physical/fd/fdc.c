/* fdc.c -- Lowest level floppy routines -
 * For handling the FDC and the DOR
 * C.Luke
 */

#define DEBUG
#include "fd.h"

/* ******** DOR routines
 * These are those ones concerned with the DOR thingy.
 * Originaly based on the mess in the Linux fd driver,
 * But later rewritten according to information found in
 * some book or other (I don't have the name here... ;)
 */

/* Last DOR value. DOR is readonly, so we need our own copy... */
static u_int current_dor;

static u_char set_dor(int drv, u_char mask, u_char data);

#define DOR_ON	0x0c
#define DOR_OFF	0x00

/* set the dor to a value. Flirble */
u_char set_dor(int drv, u_char mask, u_char data)
{
        // FIXME should probably make use of the drv to select drive
        (void)drv;
	register u_char old_dor, new_dor;

	old_dor = current_dor;
	new_dor = (old_dor & mask) | data;

	if(old_dor != new_dor)
	{
		current_dor = new_dor;
		outb_p(new_dor, FDC_DOR);
	}
	return old_dor;
}

bool select_drive(fd_dev_t *dev)
{
	/* Set the DOR to the right drive, and also set
	 * The DIR to the right rate
	 */
	static bool in = 0;

	/* Make sure we aren't recursing - the fdc_dpscify below calls
	 * us, see...
	 */
	if(in)
		return TRUE;
	in = TRUE;

	DB(("fd: select_drive %d, rate %d\n", dev->drvno, dev->disk_p->rate));
	set_dor(dev->drvno, DOR_ON | 0xf0, dev->drvno);
	outb_p(dev->disk_p->rate, FDC_DIREG);

	/* Make sure the motor is spinning... :) */
	start_motor(dev);

#if 1
	/* Send a specify.. ;) */
	fdc_specify(dev);
#endif

	in = FALSE;;
	return TRUE;
}

/* Floppy driver motor handling.. :) */
static u_char motor_count[2];

bool start_motor(fd_dev_t *dev)
{
	int i;
	DB(("fd: start_motor %d\n", dev->drvno));

	set_dor(dev->drvno, DOR_ON, FD_DRV(dev->drvno) << 4 | dev->drvno);
	i = motor_count[dev->drvno];
	motor_count[dev->drvno] = 2;
	if(!i)
		kernel->sleep_for_ticks(dev->drive_p->spin_up / 50);

	return TRUE;
}

bool stop_motor(fd_dev_t *dev)
{
	DB(("fd: stop_motor %d\n", dev->drvno));

	set_dor(dev->drvno, DOR_ON | ~(FD_DRV(dev->drvno) << 4), dev->drvno);

	return TRUE;
}

void stop_motor_test(void)
{
	int i;

	if(current_req != NULL)
		return;

	for(i=0; i<2; i++) if(motor_count[i])
	{
		motor_count[i]--;
		if(!motor_count[i])
			stop_motor(&fd_devs[i]);
	}
}

/* ******** FDC routines
 * Based largely on the Intel doc, but later revised from the
 * info in the same book as I got the DOR stuff from,
 * and the NEC Microprocessors and Peripherals Databook
 */
static u_char from_fdc[7], prev_cmd;
static int current_cyl[2];

static int get_results(int count);
static int purge_results(void);
static int send_command(u_char cmd);
static int send_param(u_char param);

int get_results(int count)
{
	u_int counter;
	int res_cnt = 0, sts;

	for(counter = 0; counter < 102400; counter++)
	{
		sts = inb_p(FDC_STATUS) & (STATUS_DIR | STATUS_READY |
			STATUS_BUSY);

		if(sts == STATUS_READY)
		{
			if(count == res_cnt)
				return E_OK;
			else
				return E_BADRESULTS;
		}

		if(sts == (STATUS_DIR | STATUS_READY | STATUS_BUSY))
		{
			if(res_cnt >= 7)
			{
				inb_p(FDC_STSREG);
				res_cnt++;
			}
			from_fdc[res_cnt++] = inb_p(FDC_STSREG);
		}
	}
	return E_BADRESULTS;
}

int purge_results()
{
	u_int counter;
	int res_cnt = 0, sts;

	for(counter = 0; counter < 102400; counter++)
	{
		sts = inb_p(FDC_STATUS) & (STATUS_DIR | STATUS_READY |
			STATUS_BUSY);

		if(sts == STATUS_READY)
			return E_OK;
		res_cnt++;
	}
	return E_BADRESULTS;
}

int send_command(u_char cmd)
{
	u_int counter;
	u_char sts;

	DB(("fd: send_command %-02.2X, purgeing..", cmd));
	purge_results();
	DB(("purged\n", cmd));

	for(counter = 0; counter < 102400; counter++)
	{
		sts = inb_p(FDC_STATUS) & (STATUS_READY | STATUS_DIR);
		if(sts == STATUS_READY)
		{
			outb_p(cmd, FDC_CMDREG);
			prev_cmd = cmd;
			DB(("T "));
			return E_OK;
		}
	}
	DB(("F "));
	return E_SENDCMD;
}

int send_param(u_char param)
{
	u_int counter;
	u_char sts;

	DB(("fd:p %-02.2X ", param));

	for(counter = 0; counter < 102400; counter++)
	{
		sts = inb_p(FDC_STATUS) & (STATUS_READY | STATUS_DIR);
		if(sts == STATUS_READY)
		{
			outb_p(param, FDC_CMDREG);
			DB(("T "));
			return E_OK;
		}
	}
	DB(("F "));
	return E_SENDCMD;
}


/* Reset the FDC */
static bool int_reset_flag;
static void reset_fdc_int(void)
{
	DB(("reset_fdc_int.."));
	int_reset_flag = TRUE;
}

bool reset_fdc(void)
{
	int i = 1000000;
	int status;

	fd_intr = reset_fdc_int;
	int_reset_flag = FALSE;
	prev_cmd = 0;

	DB(("fd: reset"));

	outb(DOR_OFF, FDC_DOR);

	kernel->udelay(20);	/* wait 20 usecs */

	current_dor = DOR_ON;
	outb(current_dor, FDC_DOR);

	while(i-- &&(int_reset_flag == FALSE));

	if(int_reset_flag == FALSE) {
		DB(("...timed out\n"));
		return FALSE;
	}

	if(fdc_sense() == FALSE) {
		DB(("...sense_int failed\n"));
		return FALSE;
	}
	status = from_fdc[0] & 0xC0;
	if((status == 0x00) || (status == 0xC0))
	{
		return TRUE;
	}
	DB(("status error\n"));
	return FALSE;
}

#define FD_DRV2(XX)	(XX)
#define CE(XX, YY)	XX |= (YY == FALSE)

/* Recalibrate the drive - move head to track 0 etc */
#define I_RECAL

#ifdef I_RECAL
static bool int_recal_flag;
static void recal_intr(void)
{
	int_recal_flag = TRUE;
}
#endif

bool fdc_recal(fd_dev_t *dev)
{
	int i = 0;
#ifdef I_RECAL
	int j = dev->drive_p->intr_timeout/FD_INTSCALE;

	int_recal_flag = FALSE;
	fd_intr = recal_intr;
#endif

	select_drive(dev);

	CE(i, send_command(FD_RECALIBRATE));
	CE(i, send_param(FD_DRV2(dev->drvno)));

#ifdef I_RECAL
	while(j-- && int_recal_flag == FALSE)
		kernel->sleep_for_ticks(FD_INTDEL);
	if(j)
		i = 1;
#endif

	current_cyl[dev->drvno] = 0;
	dev->recalibrate = 0;

#if 1
	fd_intr = NULL;
	i = (fdc_sense() == FALSE);
#endif

	fd_intr = NULL;
	if(!i)
		return TRUE;

	dev->recalibrate = 1;
	DB(("fd: error in fdc_recal\n"));
	return FALSE;
}

/* Go tell it to READ something then! */
static void read_intr(void);

bool fdc_read(fd_dev_t *dev, u_long cyl)
{
	int i = 0;

	fd_intr = read_intr;

DB(("read "));
	CE(i, send_command(FD_READ));

DB(("drv "));
	CE(i, send_param(FD_DRV2(dev->drvno)));
DB(("cyl "));
	CE(i, send_param((u_char)cyl));
DB(("head "));
	CE(i, send_param(0));  			/* We're doing both heads.. */
DB(("sector "));
	CE(i, send_param(1));				/* Sector 1 start.. */
DB(("bps "));
	CE(i, send_param(2));				/* 512 bytes/sect */
DB(("sectors "));
	CE(i, send_param(dev->disk_p->sectors));
DB(("gap "));
	CE(i, send_param(dev->disk_p->gap));
DB(("ignored "));
	CE(i, send_param(0xff));				/* God knows */

#if 0
	if(!i)
		return TRUE;

	dev->recalibrate = 1;
	DB(("fd: error in fdc_read\n"));
	return FALSE;
#else
	return TRUE;
#endif
}

/* IRQ handler for reading da floppy */
void read_intr(void)
{
	blkreq_t *req;
	fd_dev_t *dev;
	u_long big_sect, sects, track, sect, head, cyl;
	u_long start, end;

	DB(("fd:read_intr: current_req=%p\n", current_req));
	if(current_req == NULL)
		return;

	req = current_req;
	dev = REQ_FD_DEV(req);

	/* Go get the results back from the floppy drive */
	get_results(7);

	/* Look to see if we got an error of some sort.
	 * I don't think handle_error does a good enough job
	 * on reporting these things imho tho :)
	 */
	if((from_fdc[0] & ST0_INTR) != 0)
	{
		handle_error("read_intr");
		do_request(NULL);
		return;
	}

	/* Okay. Work out what we just got. It may be a full
	 * track, or it may be a partial track. Wibble.
	 */

	big_sect = req->block;
	sects = req->nblocks;

	track = big_sect / dev->disk_p->sectors;
	sect = big_sect % dev->disk_p->sectors;
	head = track % dev->disk_p->heads;
	cyl = track / dev->disk_p->heads;

	DB(("sect: %d head: %d cyl: %d\n", sect, head, cyl));

	start = sect + (head * dev->disk_p->sectors);
	if(sects > (dev->disk_p->sectors - sect))
		end = dev->disk_p->sectors - sect;
	else
		end = sects;

	{
		char *d = req->buf, *s = track_buf + start * FD_SECTSIZ;
		int count = end * FD_SECTSIZ;

		DB(("start: %d end: %d  track_buf: %p  d: %p s: %p\n",
			start, end, track_buf, d, s));

		while(count--)
			*(d++) = *(s++);
	}

	req->buf += end * FD_SECTSIZ;
	req->block += end;
	req->nblocks -= end;

	if(!req->nblocks)
	{
		/* We're finished now! Wahoo! */
		fd_end_request(0);
		do_request(NULL);
		return;
	}

	/* We have a choice here...
	 * We could stick a new request on the queue for the remainder
	 * of the job. This would look nice. :)
	 * Or we can call the diggery a bit later on.
	 * Disadvantage is then all retries then to entire request.
	 * Let's try a new request...
	 */
	prepend_node(&fd_reqs, &current_req->node);
	current_req = NULL;
	do_request(NULL);
}

/* Seek the head to the desired location */
#define I_SEEK

#ifdef I_SEEK
static bool int_seek_flag;
static void fdc_seek_int(void)
{
	int_seek_flag = TRUE;
}
#endif

bool
fdc_seek(fd_dev_t *dev, u_long cyl)
{
	int i;
#ifdef I_SEEK
	/* With 10msec delay, j=500 gives 5sec timeout */
	int j = 500/FD_INTSCALE;

	int_seek_flag = FALSE;
	fd_intr = fdc_seek_int;
#endif

	if(!cyl)
		return fdc_recal(dev);

#if 0
	/* It seems we can only seek by so many tracks at a time... */
	i = current_cyl[dev->drvno];
	i = cyl - i;
	if(k > 40)
	{
		if(fdc_seek(dev, cyl-40) == FALSE)
			return FALSE;
		if(fdc_seek(dev, cyl) == FALSE)
			return FALSE;
		return TRUE;
	}
	if(i < -40)
	{
		if(fdc_seek(dev, cyl+40) == FALSE)
			return FALSE;
		if(fdc_seek(dev, cyl) == FALSE)
			return FALSE;
		return TRUE;
	}
#endif
	i = 0;
	select_drive(dev);

	i |= (send_command(FD_SEEK) != E_OK);
	i |= (send_param(FD_DRV2(dev->drvno)) != E_OK);
	i |= (send_param(cyl) != E_OK);

#ifdef I_SEEK
	while(j-- && int_seek_flag == FALSE)
		kernel->sleep_for_ticks(FD_INTDEL);
	if(!j)
		i = 1;
#endif

	current_cyl[dev->drvno] = cyl;

	i |= (fdc_sense() == FALSE);

	fd_intr = NULL;
	if(!i)
		return TRUE;

	dev->recalibrate = 1;
	DB(("fd: error in fdc_seek "));
	return FALSE;
}

/* Go find the ID mark on the disc */
#define I_READ_ID

#ifdef I_READ_ID
static bool int_read_id_flag;
static void fdc_read_id_int(void)
{
	int_read_id_flag = TRUE;
}
#endif

bool
fdc_read_id(fd_dev_t *dev)
{
	/* With 10msec delay, j=200 gives 2sec timeout */
	int i = 0;
#ifdef I_READ_ID
	int j = 200/FD_INTSCALE;

	int_read_id_flag = FALSE;
	fd_intr = fdc_read_id_int;
#endif

	select_drive(dev);

	i |= (send_command(FD_READ_ID) != E_OK);

	i |= (send_param(FD_DRV2(dev->drvno)) != E_OK);

#ifdef I_READ_ID
	while(j-- && int_read_id_flag == FALSE)
		kernel->sleep_for_ticks(FD_INTDEL);
	if(!j)
		i = 1;
#endif

	i |= (get_results(7) != E_OK);

	fd_intr = NULL;
	if(!i)
		return TRUE;

	dev->recalibrate = 1;
	DB(("fd: error in fdc_read_id "));
	return FALSE;
}

/* Specify some stuff. egads. */
#undef I_SPECIFY

#ifdef I_SPECIFY
static bool int_specify_flag;
static void fdc_specify_int(void)
{
	int_specify_flag = TRUE;
}
#endif

bool fdc_specify(fd_dev_t *dev)
{
	int i = 0;
#ifdef I_SPECIFY
	int j = dev->drive_p->intr_timeout/FD_INTSCALE;

	int_specify_flag = FALSE;
	fd_intr = fdc_specify_int;
#endif

	select_drive(dev);

	i |= (send_command(FD_SPECIFY) != E_OK);

#if 0
	i |= (send_param((dev->drive_p->step_rate)<<4 |
		(dev->drive_p->head_unload)) != E_OK);
	i |= (send_param((dev->drive_p->head_load)<<1) != E_OK);
#else
	i |= (send_param(0x8f) != E_OK);
	i |= (send_param(0x10) != E_OK);
#endif

#ifdef I_SPECIFY
	while(j-- && int_specify_flag == FALSE)
		kernel->sleep_for_ticks(FD_INTDEL);
	if(j)
		i = 1;
#endif

	if(!i)
		return TRUE;

	dev->recalibrate = 1;
	DB(("fd: error in fdc_specify "));
	return FALSE;
}


/* Sense the interrupts status */
#undef I_SENSE

#ifdef I_SENSE
static bool int_sense_flag;
static void fdc_sense_int(void)
{
	int_sense_flag = TRUE;
}
#endif

bool fdc_sense(void)
{
#ifdef I_SENSE
	int i = 0, j = 200/FD_INTSCALE;

	int_sense_flag = FALSE;
	fd_intr = fdc_sense_int;
#endif

	send_command(FD_SENSEI);

#ifdef I_SENSE
	while(j-- && int_sense_flag == FALSE)
		kernel->sleep_for_ticks(FD_INTDEL);
	if(0&&!j)
		i = 1;
#endif

	get_results(2);
	fd_intr = NULL;

#ifdef I_SENSE
	if(!i)
#endif
		return TRUE;

#ifdef I_SENSE
	DB(("fd: error in fd_sense "));
	return FALSE;
#endif
}

/* Dump the status... used on an error.
 * We use current_req to work out the command, and print some
 * useful info on it.
 */
void dump_stat(void)
{
#define EF()	if(!(l--)) { kprintf("done.\n"); return; }

	int cmd, l, i;
	char *msg;

	cmd = prev_cmd;

	switch(cmd)
	{
	case FD_RECALIBRATE:
		msg = "Recalibrate";
		l = 0;
		break;
	case FD_READ:
		msg = "Read";
		l = 3;
		break;
	case FD_WRITE:
		msg = "Write";
		l = 3;
		break;
	case FD_SEEK:
		msg = "Seek";
		l = 1;
		break;
	case FD_READ_ID:
		msg = "Read ID";
		l = 7;
		break;
	case FD_SPECIFY:
		msg = "Specify";
		l = 0;
		break;
	case FD_SENSEI:
		msg = "Sense";
		l = 1;
		break;
	default:
		msg = "Unknown";
		l = 1;
		break;
	}
	kprintf("FDC status in %s: ", msg);

	EF();
	i = from_fdc[0];
	if(i&ST0_NR)
		kprintf("Not Ready, ");
	if(i&ST0_ECE)
		kprintf("Equipment check, ");

	EF();
	i = from_fdc[1];
	if(i&ST1_MAM)
		kprintf("Missing address mark, ");
	if(i&ST1_WP)
		kprintf("Write protect, ");
	if(i&ST1_ND)
		kprintf("Unreadable, ");
	if(i&ST1_OR)
		kprintf("Overrun, ");
	if(i&ST1_CRC)
		kprintf("CRC failed, ");
	if(i&ST1_EOC)
		kprintf("End of Cylinder, ");

	EF();
	i = from_fdc[2];
#if 1
	if(i&ST2_MAM)
		kprintf("Missing address mark, ");
#endif
	if(i&ST2_BC)
		kprintf("Bad Cylinder, ");
	if(i&ST2_SNS)
		kprintf("Scan not satisfied, ");
	if(i&ST2_SEH)
		kprintf("Scan equal hit, ");
	if(i&ST2_WC)
		kprintf("Wrong cylinder, ");
	if(i&ST2_CRC)
		kprintf("CRC failed in data, ");
	if(i&ST2_CM)
		kprintf("Data is deleted, ");
} 

