/* fd.h -- Floppy device local definitions and stuff
 * C.Luke
 */

#ifndef _FD_H
#define _FD_H

#include <vmm/types.h>
#include <vmm/kernel.h>
#include <vmm/floppy.h>
#include <vmm/fd_reg.h>
#include <vmm/dma.h>
#include <vmm/io.h>
#include <vmm/fs.h>
#include <vmm/cookie_jar.h>
#include <vmm/errno.h>

#define BLKDEV_TYPE fd_dev_t
#define BLKDEV_NAME "fd"

#include <vmm/blkdev.h>

#define kprintf kernel->printf
#define REQ_FD_DEV(r) ((fd_dev_t *)(r->dev))

/* External globals... */

extern struct drive_params main_drive_params[];
extern struct disk_params main_disk_params[];

extern char *track_buf;
extern struct DmaBuf DMAbuf;
extern fd_dev_t fd_devs[2];
extern void (*fd_intr)(void);
extern list_t fd_reqs;
extern blkreq_t *current_req;
#define FAKE_REQ ((blkreq_t *)0xFD00FD00)

extern struct timer_req timer;
extern bool int_timer_flag;

/* Function Prototypes... */

/* Request handling */
void do_request(blkreq_t *req);
int fd_sync_request(blkreq_t *req);
void fd_end_request(int i);
void handle_error(const char *from);
void timer_intr(void *);
void dump_stat(void);

/* Low level functions */
bool select_drive(fd_dev_t *dev);
bool start_motor(fd_dev_t *dev);
bool stop_motor(fd_dev_t *dev);
void stop_motor_test(void);

/* FDC functions */
bool reset_fdc(void);
bool fdc_read(fd_dev_t *, u_long cyl);
bool fdc_seek(fd_dev_t *, u_long cyl);
bool fdc_read_id(fd_dev_t *);
bool fdc_specify(fd_dev_t *);
bool fdc_recal(fd_dev_t *drive);
bool fdc_sense(void);

#endif

