/* vpic.h -- Definitions for the virtual PIC
   John Harper. */

#ifndef __VMM_VPIC_H
#define __VMM_VPIC_H

#include <vmm/types.h>
#include <vmm/vm.h>

enum pic_state {
    Normal, WaitICW2, WaitICW3, WaitICW4, ReadISR, ReadIRR
};

struct vpic {
    u_char base, mask, irr, isr;
    bool need_icw4;
    enum pic_state state;
    bool is_slave;
    u_char link;
};

struct vpic_pair {
    struct vpic master, slave;
    struct vm_kill_handler kh;
};

struct vpic_module {
    struct vxd_module base;
    void (*simulate_irq)(struct vm *vm, u_char irq);
    void (*IF_enabled)(struct vm *vm);
    void (*IF_disabled)(struct vm *vm);
    void (*set_mask)(struct vm *vm, bool set, u_short mask);
};

extern struct vpic_module vpic_module;

#endif /* __VMM_VPIC_H */
