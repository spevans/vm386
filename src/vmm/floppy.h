#include <vmm/types.h>
#include <vmm/kernel.h>



/* Request commands */

#define FD_CMD_TIMER	1
#define FD_CMD_READ	2
#define FD_CMD_WRITE	3
#define FD_CMD_SEEK	4

/* Error Codes */

#define E_OK            0
#define E_BADSEEK       1
#define E_BADRECAL      2
#define E_BADRESULTS    3
#define E_NOINT         4
#define E_SENDCMD       5
#define E_BADFDCSTS     6
#define E_BADCMD        7
#define E_READYLINE     8
#define E_SECTNOTFOUND  9
#define E_CRC           10
#define E_OVERRUN       11
#define E_READONLY      12
#define E_ADDRMARK      13
#define E_BADFDC        14
#define E_ENDOFCYLINDER 15
#define E_NEWMEDIA      16

/* Magic Numbers */

#define MAX_RETRIES	1
#define RECAL_FREQ	16	
#define TRACKBUF_PAGES	10	/* 40k track buffer */

struct drive_params {
    int cmos_type;
    int max_tracks;
    int head_load;
    int head_unload;
    int step_rate;
    int spin_up;
    int intr_timeout;
    int native_format;
    int other_formats[8];
    char *name;
};


struct disk_params {
    u_int	blocks,
		sectors,
		heads,
		tracks,
		stretch;
    u_char	gap,
		rate,
		spec1,
		fmt_gap;
    char	*name;
};


typedef struct {
    struct drive_params *drive_p;
    struct disk_params *disk_p; 
    const char *name;
    int drvno;
    u_long total_blocks;
    bool recalibrate;
    /* ... */
} fd_dev_t;


/* Commands */

extern bool floppy_init(void);

extern bool floppy_add_dev(fd_dev_t *fd);
extern bool floppy_remove_dev(void);

extern long floppy_read_blocks(fd_dev_t *fd, void *buf, u_long block, int count);
extern long floppy_write_blocks(fd_dev_t *fd, void *buf, u_long block, int count);
extern long floppy_test_media(void *f);
extern bool floppy_mount_partition(void);
extern bool floppy_mkfs_partition(void);

extern long floppy_force_seek(fd_dev_t *fd, u_long cyl);

extern bool add_floppy_commands(void);
extern bool floppy_init(void);

struct fd_module {
    struct module base;

    bool (*floppy_add_dev)(fd_dev_t *fd);
    bool (*floppy_remove_dev)(void);
    long (*floppy_read_blocks)(fd_dev_t *fd, void *buf, u_long block, int count);
    long (*floppy_write_blocks)(fd_dev_t *fd, void *buf, u_long block, int count);
    bool (*floppy_mount_partition)(void);
    bool (*floppy_mkfs_partition)(void);
    long (*floppy_force_seek)(fd_dev_t *fd, u_long cyl);
};

