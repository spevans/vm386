/* vcmos.h -- Definitions for the virtual CMOS. */

#ifndef __VMM_VCMOS_H
#define __VMM_VCMOS_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/vm.h>
#include <vmm/time.h>

/* structure of a cmos chip */

struct vcmos {
    struct vm *vm;
    u_char cmos_mem[64];
    struct vm_kill_handler kh;
    u_char cmos_reg;
    struct timer_req timer;
};

struct vcmos_module {
    struct vxd_module base;
    void (*delete_vcmos)(struct vm *vm);
    u_char (*get_vcmos_byte)(struct vm *vm, u_int addr);
    void (*set_vcmos_byte)(struct vm *vm, u_int addr, u_char val);
    void (*get_bcd_time)(struct vm *vm, struct vm86_regs *regs);
    void (*set_bcd_time)(struct vm *vm, struct vm86_regs *regs);
    void (*get_bcd_date)(struct vm *vm, struct vm86_regs *regs);
    void (*set_bcd_date)(struct vm *vm, struct vm86_regs *regs);
    void (*set_alarm)(struct vm *vm, struct vm86_regs *regs);
    void (*reset_alarm)(struct vm *vm, struct vm86_regs *regs);
};

extern void delete_vcmos(struct vm *vm);
extern u_char get_vcmos_byte(struct vm *vm, u_int addr);
extern void set_vcmos_byte(struct vm *vm, u_int addr, u_char val);
extern void get_bcd_time(struct vm *vm, struct vm86_regs *regs);
extern void set_bcd_time(struct vm *vm, struct vm86_regs *regs);
extern void get_bcd_date(struct vm *vm, struct vm86_regs *regs);
extern void set_bcd_date(struct vm *vm, struct vm86_regs *regs);
extern void set_alarm(struct vm *vm, struct vm86_regs *regs);
extern void reset_alarm(struct vm *vm, struct vm86_regs *regs);

extern struct vcmos_module vcmos_module;

#define	CMOS_ADDR_LOW	0x70
#define CMOS_ADDR_HI	0x71
#define CMOS_REG_PORT	0x70
#define CMOS_DATA_PORT	0x71
#define NMI_SET_BIT     0x80


#endif /* __VMM_VCMOS_H */
