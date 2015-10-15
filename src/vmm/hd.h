/* hd.h -- Definitions for all kinds of hard disks.
   John Harper. */

#ifndef _VMM_HD_H
#define _VMM_HD_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/fs.h>

#define PARTN_NAME_MAX 8

typedef struct hd_partition {
    struct hd_partition *next;
    u_long start;
    u_long size;
    struct hd_dev *hd;
    char name[PARTN_NAME_MAX];
} hd_partition_t;

typedef struct hd_dev {
    struct hd_dev *next;
    const char *name;
    u_long heads, cylinders, sectors;
    u_long total_blocks;
    bool (*read_blocks)(struct hd_dev *hd, void *buf, u_long block,
			int count);
    bool (*write_blocks)(struct hd_dev *hd, void *buf, u_long block,
			 int count);
} hd_dev_t;

struct hd_module {
    struct module base;

    bool (*add_dev)(hd_dev_t *dev);
    bool (*remove_dev)(hd_dev_t *dev);

    hd_partition_t *(*find_partition)(const char *name);
    bool (*read_blocks)(hd_partition_t *p, void *buf, u_long block, int count);
    bool (*write_blocks)(hd_partition_t *p, void *buf, u_long block,
			 int count);
    bool (*mount_partition)(hd_partition_t *p, bool read_only);
    bool (*mkfs_partition)(hd_partition_t *p, u_long reserved);
};


/* Function prototypes. */

#ifdef HD_MODULE

/* from ide.c */
extern bool ide_read_blocks(hd_dev_t *hd, void *buf, u_long block, int count);
extern bool ide_write_blocks(hd_dev_t *hd, void *buf, u_long block, int count);
extern void ide_init(void);

/* from generic.c */
extern hd_dev_t *hd_list;
extern hd_partition_t *partition_list;
extern void init_partitions(struct fs_module *fs);
extern hd_partition_t *hd_find_partition(const char *name);
extern bool hd_add_dev(hd_dev_t *hd);
extern bool hd_remove_dev(hd_dev_t *hd);
extern bool hd_read_blocks(hd_partition_t *p, void *buf, u_long block, int count);
extern bool hd_write_blocks(hd_partition_t *p, void *buf, u_long block, int count);
extern bool hd_mount_partition(hd_partition_t *p, bool read_only);
extern bool hd_mkfs_partition(hd_partition_t *p, u_long reserved);

/* from hd_mod.c */
extern struct hd_module hd_module;
extern bool hd_init(void);

/* from cmds.c */
extern bool add_hd_commands(struct fs_module *fs);

#endif /* HD_MODULE */
#endif /* _VMM_HD_H */
