#include <vmm/types.h>
#include <vmm/kernel.h>

#define FLOPPY_IRQ      6
#define FLOPPY_DMA      2
#define FDC_DOR         0x3F2
#define FDC_STATUS      0x3F4
#define FDC_CMDREG      0x3F5
#define FDC_STSREG      0x3F5
#define FDC_DIREG       0x3F7

#define FD_SECTSIZ	512
#define FD_TIMEINT	54
#define FD_INTDEL	1
#define FD_INTSCALE	5
#define FD_DRV(XX)	(1 << XX)

/* Bits of main status register */
#define STATUS_BUSYMASK	0x0F		/* drive busy mask */
#define STATUS_BUSY	0x10		/* FDC busy */
#define STATUS_DMA	0x20		/* 0- DMA mode */
#define STATUS_DIR	0x40		/* 0- cpu->fdc */
#define STATUS_READY	0x80		/* Data reg ready */

/* Bits of FD_ST0 */
#define ST0_DS		0x03		/* drive select mask */
#define ST0_HA		0x04		/* Head (Address) */
#define ST0_NR		0x08		/* Not Ready */
#define ST0_ECE		0x10		/* Equipment chech error */
#define ST0_SE		0x20		/* Seek end */
#define ST0_INTR	0xC0		/* Interrupt code mask */

/* Bits of FD_ST1 */
#define ST1_MAM		0x01		/* Missing Address Mark */
#define ST1_WP		0x02		/* Write Protect */
#define ST1_ND		0x04		/* No Data - unreadable */
#define ST1_OR		0x10		/* OverRun */
#define ST1_CRC		0x20		/* CRC error in data or addr */
#define ST1_EOC		0x80		/* End Of Cylinder */

/* Bits of FD_ST2 */
#define ST2_MAM		0x01		/* Missing Addess Mark (again) */
#define ST2_BC		0x02		/* Bad Cylinder */
#define ST2_SNS		0x04		/* Scan Not Satisfied */
#define ST2_SEH		0x08		/* Scan Equal Hit */
#define ST2_WC		0x10		/* Wrong Cylinder */
#define ST2_CRC		0x20		/* CRC error in data field */
#define ST2_CM		0x40		/* Control Mark = deleted */

/* Bits of FD_ST3 */
#define ST3_HA		0x04		/* Head (Address) */
#define ST3_TZ		0x10		/* Track Zero signal (1=track 0) */
#define ST3_WP		0x40		/* Write Protect */

/* Values for FD_COMMAND */
#define FD_RECALIBRATE		0x07	/* move to track 0 */
#define FD_SEEK			0x0F	/* seek track */
#define FD_READ			0xE6	/* read with MT, MFM, SKip deleted */
#define FD_WRITE		0xC5	/* write with MT, MFM */
#define FD_SENSEI		0x08	/* Sense Interrupt Status */
#define FD_SPECIFY		0x03	/* specify HUT etc */
#define FD_FORMAT		0x4D	/* format one track */
#define FD_VERSION		0x10	/* get version code */
#define FD_CONFIGURE		0x13	/* configure FIFO operation */
#define FD_PERPENDICULAR	0x12	/* perpendicular r/w mode */
#define FD_READ_ID		0x46	/* find index hole*/

/* FDC version return types */
#define FDC_TYPE_STD	0x80	/* normal 8272A clone FDC */
#define FDC_TYPE_82077	0x90	/* FIFO + perpendicular support */

#define BIT7 		0x80
#define BIT6 		0x40
#define BIT5 		0x20
#define BIT4 		0x10
#define BIT3 		0x08
#define BIT2 		0x04
#define BIT1 		0x02
#define BIT0 		0x01


#define RATE_500 		0x00
#define RATE_300 		0x01
#define RATE_250 		0x02
#define RATE_1000               0x03
