/* hdreg.h -- Hardware definitions for the hard disk (IDE).
   John Harper. */

#ifndef _VMM_HDREG_H
#define _VMM_HDREG_H


/* I/O ports. These are for the first controller, the other one can be
   accessed at 0x170-0x177. */

#define HD_DATA		0x1f0
#define HD_ERROR	0x1f1		/* for reading */
#define HD_FEATURE	HD_ERROR	/* for writing */
#define HD_NSECTOR	0x1f2		/* # of sectors to read/write */
#define HD_SECTOR	0x1f3		/* starting sector */
#define HD_LCYL		0x1f4		/* starting cylinder */
#define HD_HCYL		0x1f5		/* high byte */
#define HD_CURRENT	0x1f6		/* 101dhhhh, d=drive, hhhh=head */
#define HD_STATUS	0x1f7		/* Get status, reset INTRQ. */
#define HD_COMMAND	HD_STATUS	/* Invoke command. */

#define HD_DEVCTRL	0x3f6		/* Device control. See bits below. */
#define HD_ALTSTATUS	HD_DEVCTRL	/* Get status, don't reset INTRQ. */
#define HD_DRVADDR	0x3f7		/* Get drive address info. */


/* Values. */

/* HD_ERROR bits. */
#define HD_ERR_AMNF	0x01		/* Address mark not found. */
#define HD_ERR_TK0NF	0x02		/* Track zero not found. */
#define HD_ERR_ABRT	0x04		/* Aborted command. */
#define HD_ERR_MCR	0x08		/* Media change requested. */
#define HD_ERR_IDNF	0x10		/* ID not found. */
#define HD_ERR_MC	0x20		/* Media changed. */
#define HD_ERR_UNK	0x40		/* Uncorrectable data error. */
#define HD_ERR_BBK	0x80		/* Bad block detect. */

/* HD_STATUS bits. */
#define HD_STAT_ERR	0x01		/* Error. */
#define HD_STAT_IDX	0x02		/* Index (set once per revolution). */
#define HD_STAT_CORR	0x04		/* Corrected data. */
#define HD_STAT_DRQ	0x08		/* Data request. */
#define HD_STAT_DSC	0x10		/* Drive seek complete. */
#define HD_STAT_DWF	0x20		/* Drive write fault. */
#define HD_STAT_DRDY	0x40		/* Drive ready. */
#define HD_STAT_BSY	0x80		/* Busy. */

/* HD_DEVCTRL bits. */
#define HD_nIEN		0x02		/* Disable IRQ. */
#define HD_SRST		0x04		/* Host software reset. */
#define HD_4_BIT_HEADS	0x08		/* Head addressing mode. (Set to 1?) */

/* HD_COMMAND values. */
#define HD_CMD_RECAL		0x10	/* Recalibrate device. */
#define HD_CMD_READ		0x20	/* Read sector(s) (w/ retry). */
#define HD_CMD_WRITE		0x30	/* Write sectors(s) (w/ retry). */
#define HD_CMD_VERIFY		0x40	/* Read verify sector(s) (w/ retry). */
#define HD_CMD_FORMAT		0x50	/* Format track. */
#define HD_CMD_SEEK		0x70	/* Seek. */
#define HD_CMD_DIAGNOSE		0x90	/* Execute drive diagnostic. */
#define HD_CMD_SPECIFY		0x91	/* Initialise drive parameters. */
#define HD_CMD_SETIDLE1		0xE3	/* First byte of Idle. */
#define HD_CMD_SETIDLE2		0x97	/* Second byte of Idle. */
#define HD_CMD_READMULT		0xC4	/* Read multiple. */
#define HD_CMD_WRITEMULT	0xC5	/* Write multiple. */
#define HD_CMD_SETMULT		0xC6	/* Set multiple mode. */
#define HD_CMD_IDENTIFY		0xEC	/* Identify drive. */
#define HD_CMD_SETFEATURES	0xEF	/* Set features. */

#endif /* _VMM_HDREG_H */
