/* vm.h -- Virtual machine definitions.
   John Harper. */

#ifndef __VMM_VM_H
#define __VMM_VM_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/page.h>
#include <vmm/cookie_jar.h>
#include <vmm/time.h>

struct vm86_regs {
    u_long	eax;
    u_long	ebx;
    u_long	ecx;
    u_long	edx;	
    u_long	edi;
    u_long	esi;
    u_long	ebp;
    u_short	_ds, __dsfill;
    u_short	_es, __esfill;
    u_short	_fs, __fsfill;
    u_short	_gs, __gsfill;
    u_long	error_code;
    u_long	eip;
    u_short	cs, _csfill;
    u_long	eflags;
    u_long	esp;
    u_short	ss, _ssfill;
    u_short	es, _esfill;
    u_short	ds, _dsfill;
    u_short	fs, _fsfill;
    u_short	gs, _gsfill;
};

/* Forward decl. */
struct vm;

struct io_handler {
    struct io_handler *next;
    const char *name;
    u_short low_port, high_port;
    u_long (*in)(struct vm *vm, u_short port, int size);
    void (*out)(struct vm *vm, u_short port, int size, u_long val);
};

struct arpl_handler {
    struct arpl_handler *next;
    const char *name;
    u_short low, high;
    void (*func)(struct vm *vm, struct vm86_regs *regs, u_short svc);
};

struct vm_kill_handler {
    struct vm_kill_handler *next;
    void (*func)(struct vm *vm);
};


/* Virtual machines. */

struct vm {
    struct task *task;
    struct io_handler *local_io;
    struct tty *tty;
    u_long virtual_eflags;
    struct vm_kill_handler *kill_list;
    bool hlted;
    bool nmi_sts;
    bool a20_state;
    u_long himem_ptes[16];
    void *slots[32];
    struct cookie_jar hardware;
};

#define GET_TASK_VM(task) ((struct vm *)((task)->user_data))

#define EFLAGS_RF	0x00010000
#define EFLAGS_VM	0x00020000
#define FLAGS_NT	0x8000
#define FLAGS_IOPL	0x7000
#define FLAGS_DF	0x0400
#define FLAGS_IF	0x0200
#define FLAGS_TF	0x0100
#define FLAGS_CF	0x0001
#define USER_EFLAGS	0x00000cff

#define STC(regs)	((regs)->eflags |= FLAGS_CF)
#define CLC(regs)	((regs)->eflags &= ~FLAGS_CF)

#define NMI_DISABLED	0
#define NMI_ENABLED	1

#define GET16(x)   ((x) & 0xffff)
#define SET16(x,v) (((x) & 0xffff0000) | ((v) & 0xffff))
#define GET8(x)	   ((x) & 0xff)
#define SET8(x,v)  (((x) & 0xffffff00) | ((v) & 0xff))
#define GET8H(x)   (((x) & 0xff00) >> 8)
#define SET8H(x,v) (((x) & 0xffff00ff) | (((v) & 0xff) << 8))


struct vm_module {
    struct module base;

    struct vm *(*create_vm)(const char *name, u_long virtual_mem,
			    const char *display_type);
    void (*kill_vm)(struct vm *vm);

    void (*add_io_handler)(struct vm *local, struct io_handler *ioh);
    void (*remove_io_handler)(struct vm *local, struct io_handler *ioh);
    struct io_handler *(*get_io_handler)(struct vm *vm, u_short port);

    void (*add_arpl_handler)(struct arpl_handler *ah);
    void (*remove_arpl_handler)(struct arpl_handler *ah);
    struct arpl_handler *(*get_arpl_handler)(u_short svc);

    void (*add_vm_kill_handler)(struct vm *vm, struct vm_kill_handler *kh);

    int (*alloc_vm_slot)(void);
    void (*free_vm_slot)(int slot);
    void (*set_gate_a20)(struct vm *vm, bool state);
    void (*simulate_int)(struct vm *vm, struct trap_regs *regs, int type);
};

extern struct vm_module vm_module;

struct vxd_module {
    struct module vxd_base;
    bool (*create_vxd)(struct vm *vm, int argc, char **argv);
};


/* Protoypes. */

#ifdef VM_MODULE

/* Forward references. */
struct shell;

/* from vmach.c */
extern struct io_handler *global_io;
extern bool verbose_io;
extern bool init_vm(void);
extern struct vm *create_vm(const char *name, u_long virtual_mem,
			    const char *display_type);
extern void kill_vm(struct vm *vm);
extern void add_io_handler(struct vm *local, struct io_handler *ioh);
extern void remove_io_handler(struct vm *local, struct io_handler *ioh);
extern struct io_handler *get_io_handler(struct vm *vm, u_short port);
extern void describe_vm_io(struct shell *sh, struct vm *vm);
extern void add_arpl_handler(struct arpl_handler *ah);
extern void remove_arpl_handler(struct arpl_handler *ah);
extern struct arpl_handler *get_arpl_handler(u_short svc);
extern void add_vm_kill_handler(struct vm *vm, struct vm_kill_handler *kh);
extern void describe_arpls(struct shell *sh);
extern int alloc_vm_slot(void);
extern void free_vm_slot(int slot);
extern void set_gate_a20(struct vm *vm, bool state);

/* from fault.c */
extern void set_bios_handler(void (*bh)(struct vm *, u_char));
extern void simulate_vm_int(struct vm *vm, struct trap_regs *regs, int vector);
extern void vm_gpe_handler(struct trap_regs *regs);
extern void vm_invl_handler(struct trap_regs *regs);
extern void vm_debug_handler(struct trap_regs *regs);
extern void vm_div_handler(struct trap_regs *regs);
extern void vm_breakpoint_handler(struct trap_regs *regs);
extern void vm_ovfl_handler(struct trap_regs *regs);
extern bool vm_pfl_handler(struct trap_regs *regs, u_long lin_addr);

/* from inslen.c */
extern size_t get_inslen(u_char *pc, bool in_user, bool is_32bit);

/* from test.c */
extern bool add_vm_commands(void);

/* from glue.c */
extern void add_glue_commands(void);

#endif /* VM_MODULE */
#endif /* __VMM_VM_H */
