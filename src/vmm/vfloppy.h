/* vfloppy.h -- Definitions for the virtual floppy driver.
   Simon Evans. */

#ifndef __VMM_V_FLOPPY_H
#define __VMM_V_FLOPPY_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/vm.h>

/* Forward references. */
struct vm;

struct vfloppy_module {
    struct vxd_module base;

    void (*delete)(struct vm *vm);
    int (*read_user_blocks)(struct vm *vm, u_int drvno, u_int head, u_int cyl,
			    u_int sector, int count, void *buf);
#if 0
    int (*write_user_blocks)(struct vm *vm, u_int drvno, u_int head,
			     u_int cyl, u_int sector, int count, void *buf);
#endif
    bool (*get_status)(struct vm *vm, u_char *statp, u_char *errp);
};

extern void kill_vfloppy(struct vm *vm);
extern bool change_vfloppy(struct vm *vm, const char *new_file);
extern int vfloppy_read_sectors(struct vm *vm, u_int drvno, u_int head, u_int cyl,
                            u_int sector, int count, void *buf);
 
extern struct shell_cmds vfloppy_cmds;

extern struct vfloppy_module vfloppy_module;

#endif /* __VMM_V_FLOPPY_H */
