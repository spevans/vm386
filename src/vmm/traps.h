/* defs for trap numbers */

#ifndef __VMM_TRAPS_H
#define __VMM_TRAPS_H

#include <vmm/types.h>

#define	DIV_ERROR	0
#define DEBUG_EXCP	1
#define	NMI		2
#define	SINGLE_DEBUG	3
#define	OVERFLOW	4
#define	BOUNDS		5
#define	INVL_OPCODE	6
#define	DNA		7
#define	DOUBLE_FAULT	8
#define	COPRO_OVERRUN	9
#define	INVALID_TSS	10
#define	SEG_NOT_PRES	11
#define	STACK_SEG	12
#define GEN_PROT	13
#define	PAGE_EXCPT	14
#define	CO_PRO_ERR	16 

struct trap_regs {
	unsigned int	eax;		/* 0x00 */
	unsigned int	ebx;		/* 0x04 */
	unsigned int	ecx;		/* 0x08 */
	unsigned int	edx;		/* 0x0c */
	unsigned int	edi;		/* 0x10 */
	unsigned int	esi;		/* 0x14 */
	unsigned int	ebp;		/* 0x18 */
	unsigned short	ds, _dsfill;	/* 0x1c */
	unsigned short	es, _esfill;	/* 0x20 */
	unsigned short	fs, _fsfill;	/* 0x24 */
	unsigned short	gs, _gsfill;	/* 0x28 */
	unsigned int	error_code;	/* 0x2c */
	unsigned int	eip;		/* 0x30 */
	unsigned short	cs, _csfill;	/* 0x34 */
	unsigned int	eflags;		/* 0x38 */
	unsigned int	esp;		/* 0x3c */
	unsigned short	ss, _ssfill;	/* 0x40 */
};


/* Debugging reg. stuff. */
#define DBUG_FETCH	0
#define DBUG_WRITE	1
#define DBUG_RDWR	3
#define DBUG_ONE	0
#define DBUG_TWO	1
#define DBUG_FOUR	3

#define DBUG_BT		0x8000
#define DBUG_BS		0x4000
#define DBUG_BD		0x2000

#define GET_DR(x)				\
extern inline u_long				\
get_dr ## x ## (void)				\
{						\
    u_long res;					\
    asm ("movl %%db" #x ",%0" : "=r" (res));	\
    return res;					\
}

GET_DR(7)
GET_DR(6)
GET_DR(3)
GET_DR(2)
GET_DR(1)
GET_DR(0)

#define SET_DR(x)				\
extern inline void				\
set_dr ## x ## (u_long val)			\
{						\
    asm ("movl %0,%%db" #x : : "r" (val));	\
}

SET_DR(7)
SET_DR(6)
SET_DR(3)
SET_DR(2)
SET_DR(1)
SET_DR(0)

extern void set_debug_reg(int reg_set, u_long addr, int type, int size);



#ifdef KERNEL

extern void dump_regs(struct trap_regs *regs, bool halt);

extern void init_trap_gates(void);	
extern void divide_error(void);
extern void debug_exception(void);
extern void nmi(void);
extern void single_debug(void);
extern void overflow(void);
extern void bounds(void);
extern void invl_opcode(void);
extern void dna(void);
extern void double_fault(void);
extern void copro_overrun(void);
extern void invalid_tss(void);
extern void seg_not_pres(void);
extern void stack_seg(void);
extern void gen_prot(void);
extern void page_exception(void);
extern void co_pro_err(void);

#endif /* KERNEL */	
#endif /* __VMM_TRAPS_H */
