/* v-ide.h -- Definitions for the virtual IDE driver.
   John Harper. */

#ifndef __VMM_V_IDE_H
#define __VMM_V_IDE_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/vm.h>

/* Forward references. */
struct vm;

struct vide_module {
    struct vxd_module base;

    void (*delete)(struct vm *vm);
    int (*read_user_blocks)(struct vm *vm, u_int drvno, u_int head, u_int cyl,
			    u_int sector, int count, void *buf);
    int (*write_user_blocks)(struct vm *vm, u_int drvno, u_int head,
			     u_int cyl, u_int sector, int count, void *buf);
    bool (*get_status)(struct vm *vm, u_char *statp, u_char *errp);
    bool (*get_geom)(struct vm *vm, u_int *headsp, u_int *cylsp,
		     u_int *sectsp);
};

extern struct vide_module vide_module;

#endif /* __VMM_V_IDE_H */
