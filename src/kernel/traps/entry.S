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

TRAP_STUB(divide_error)
TRAP_STUB(debug_exception)
TRAP_STUB(nmi)
TRAP_STUB(single_debug)
TRAP_STUB(overflow)
TRAP_STUB(bounds)
TRAP_STUB(invl_opcode)
TRAP_STUB(dna)
TRAP_STUB_EC(double_fault)
TRAP_STUB(copro_overrun)
TRAP_STUB_EC(invalid_tss)
TRAP_STUB_EC(seg_not_pres)
TRAP_STUB_EC(stack_seg)
TRAP_STUB_EC(gen_prot)
TRAP_STUB_EC(page_exception)
TRAP_STUB(co_pro_err)

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
	cmpl	$0,intr_nest_count
	jne	1f
	cmpb	$0,need_resched
	je	1f
	call	schedule
1:
	/* Check the return_hook */
	cmpl	$0,intr_nest_count
	jne	1f
	cli
	movl	current_task,%eax
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
