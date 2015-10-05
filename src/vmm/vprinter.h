/* vprinter.h -- Definitions for the virtual printer ports. */

#ifndef __VMM_VPRINTER_H
#define __VMM_VPRINTER_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/vm.h>
#include <vmm/fs.h>

#define	MAX_LPR_OFFSET	2

/* structure for all the printers for one virtual machine */

struct vprinter {
    struct vm *vm;
    struct vm_kill_handler kh;
    u_int total_ports;
    u_short base_addr[4];
    struct file *spool_file[4];
    struct io_handler ports[4];
    u_short last_data_val[4];
};

struct vprinter_module {
    struct vxd_module base;
    void (*delete)(struct vm *vm);
    void (*printer_write_char)(struct vm *vm, struct vm86_regs *regs);
    void (*printer_initialise)(struct vm *vm, struct vm86_regs *regs);
    void (*printer_get_status)(struct vm *vm, struct vm86_regs *regs);
};
extern struct vprinter_module vprinter_module;

#endif /* __VMM_VPRINTER_H */
