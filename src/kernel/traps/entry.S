#define __ASM__

#include <vmm/segment.h>

#define SAVE_REGS				\
	cld;					\
	push	%gs;				\
	push	%fs;				\
	push	%es;				\
	push	%ds;				\
	pushl	%ebp;				\
	pushl	%esi;				\
	pushl	%edi;				\
	pushl	%edx;				\
	pushl	%ecx;				\
	pushl	%ebx;				\
	pushl	%eax;				\
	movw	$(KERNEL_DATA), %dx;		\
	movw	%dx, %ds;			\
	movw	%dx, %es;			\
	movw	$(USER_DATA), %dx;		\
	movw	%dx, %fs

#define	RESTORE_REGS					\
	popl	%eax;					\
	popl	%ebx;					\
	popl	%ecx;					\
	popl	%edx;					\
	popl	%edi;					\
	popl	%esi;					\
	popl	%ebp;					\
	pop	%ds;					\
	pop	%es;					\
	pop	%fs;					\
	pop	%gs;					\
	addl	$4,%esp	/* Pop error code. */
	
#define TRAP_STUB(name)				\
	.globl	name;				\
	.align	2;				\
name ## :					\
	pushl	$0xDEAD03C0;			\
	SAVE_REGS;				\
	movl	$ ## name ## _handler,%eax;	\
	jmp	_do_exception_handling

#define TRAP_STUB_EC(name)			\
	.globl	name;				\
	.align	2;				\
name ## :					\
	SAVE_REGS;				\
	movl	$ ## name ## _handler,%eax;	\
	jmp	_do_exception_handling

TRAP_STUB(_divide_error)
TRAP_STUB(_debug_exception)
TRAP_STUB(_nmi)
TRAP_STUB(_single_debug)
TRAP_STUB(_overflow)
TRAP_STUB(_bounds)
TRAP_STUB(_invl_opcode)
TRAP_STUB(_dna)
TRAP_STUB_EC(_double_fault)
TRAP_STUB(_copro_overrun)
TRAP_STUB_EC(_invalid_tss)
TRAP_STUB_EC(_seg_not_pres)
TRAP_STUB_EC(_stack_seg)
TRAP_STUB_EC(_gen_prot)
TRAP_STUB_EC(_page_exception)
TRAP_STUB(_co_pro_err)

/* Offsets into task structure */
#define RETURN_HOOK 8

/* Main entry point for all exceptions.
   %eax = C exception handler to call.  */
	.align 2
_do_exception_handling:

	movl	%esp,%edx
	pushl	%edx
	/* Note that this function must preserve %edi, %esi and %ebp;
	   any C function does this by default. */
	call	*%eax
	addl	$4,%esp

	/* If possible, and it's required, call schedule(). */
	cmpl	$0,_intr_nest_count
	jne	1f
	cmpb	$0,_need_resched
	je	1f
	call	_schedule
1:
	/* Check the return_hook */
	cmpl	$0,_intr_nest_count
	jne	1f
	cli
	movl	_current_task,%eax
	movl	RETURN_HOOK(%eax),%eax
	testl	%eax,%eax
	jz	1f
	movl	%esp,%ebx
	pushl	%ebx
	call	*%eax
	addl	$4,%esp
1:
	RESTORE_REGS
	iret
