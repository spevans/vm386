/* vpit.h -- Definitions for the virtual timer
   John Harper. */

#ifndef __VMM_VPIT_H
#define __VMM_VPIT_H

#include <vmm/types.h>
#include <vmm/vm.h>

enum vpit_latch_state {
    counter_low, counter_high, status
};

struct vpit_channel {
    u_char command, latch_state;
    u_short divisor, interval_ticks;
    u_long start_ticks;
};

struct vpit {
    struct vpit_channel channels[3];
    struct timer_req chan0_timer;
    bool timer_ticking;
    void (*chan2_callback)(struct vm *vm);
    struct vm_kill_handler kh;
};

struct vpit_module {
    struct vxd_module base;
    struct vpit *(*get_vpit)(struct vm *vm);
};

extern struct vpit_module vpit_module;

#endif /* __VMM_VPIT_H */
