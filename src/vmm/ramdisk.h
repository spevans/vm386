#ifndef __VMM_RAMDISK_H__
#define __VMM_RAMDISK_H__

#include <vmm/types.h>
#include <vmm/kernel.h>
#include <vmm/lists.h>

#define RD_CMD_READ	1
#define RD_CMD_WRITE	2

typedef struct {
    list_node_t	node;
    char name[10];
    int drvno;
    u_long total_blocks;
    char *ram;
} rd_dev_t;


/* Commands */

extern bool ramdisk_init(void);
extern bool ramdisk_add_dev(void);
extern bool ramdisk_remove_dev(void);

extern long ramdisk_read_blocks(rd_dev_t *fd, void *buf, u_long block, int count);
extern long ramdisk_write_blocks(rd_dev_t *fd, void *buf, u_long block, int count);
extern long ramdisk_test_media(void *f);
extern bool ramdisk_mount_disk(rd_dev_t *rd);
extern bool ramdisk_mkfs_disk(rd_dev_t *rd, u_long reserved);

extern bool add_ramdisk_commands(void);
extern void remove_ramdisk_commands(void);
extern bool ramdisk_init(void);

extern rd_dev_t *create_ramdisk(u_long blocks);
extern bool delete_ramdisk(rd_dev_t *rd);

struct ramdisk_module {
    struct module base;

    bool (*ramdisk_add_dev)(void);
    bool (*ramdisk_remove_dev)(void);
    long (*ramdisk_read_blocks)(rd_dev_t *fd, void *buf, u_long block, int count);
    long (*ramdisk_write_blocks)(rd_dev_t *fd, void *buf, u_long block, int count);
    bool (*ramdisk_mount_disk)(rd_dev_t *rd);
    bool (*ramdisk_mkfs_disk)(rd_dev_t *rd, u_long reserved);
    rd_dev_t *(*create_ramdisk)(u_long blocks);
    bool (*delete_ramdisk)(rd_dev_t *rd);

    bool (*add_commands)(void);
};

extern struct ramdisk_module ramdisk_module;
extern bool ramdisk_init(void);

#endif /* __VMM_RAMDISK_H__ */

