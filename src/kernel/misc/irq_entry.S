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
	movw	%dx, %fs;

#define	RESTORE_REGS				\
	popl	%eax;				\
	popl	%ebx;				\
	popl	%ecx;				\
	popl	%edx;				\
	popl	%edi;				\
	popl	%esi;				\
	popl	%ebp;				\
	pop	%ds;				\
	pop	%es;				\
	pop	%fs;				\
	pop	%gs;				\
	leal	4(%esp),%esp	/* Pop ec */

#define LOW_IRQ(n)				\
	.globl __irq ## n;			\
	.align 2;				\
__irq ## n:;					\
	pushl	$0xDEAD1234;	/* Fake ec */	\
	SAVE_REGS;				\
	pushl	$ ## n;				\
	jmp	_do_irq

LOW_IRQ(0)
LOW_IRQ(1)
LOW_IRQ(2)
LOW_IRQ(3)
LOW_IRQ(4)
LOW_IRQ(5)
LOW_IRQ(6)
LOW_IRQ(7)
LOW_IRQ(8)
LOW_IRQ(9)
LOW_IRQ(10)
LOW_IRQ(11)
LOW_IRQ(12)
LOW_IRQ(13)
LOW_IRQ(14)
LOW_IRQ(15)

/* Task structure offsets. */
#define RETURN_HOOK 8

.align 2
_do_irq:
	incl	_intr_nest_count

	movl	(%esp),%ebx
	movl	_Irq_handlers(,%ebx,4), %ebx
	orl	%ebx, %ebx
	jz	_no_func
	call	*%ebx
_no_func:
	movb	$0x20,%al
	popl	%ebx
	cmpl	$7,%ebx
	jle	low_pic
	outb	%al, $0xA0
low_pic:
	outb	%al, $0x20

	decl	_intr_nest_count
	jnz	1f
	cmpb	$0,_need_resched
	je	1f
	call	_schedule
1:
	/* Check the return_hook */
	cmpl	$0,_intr_nest_count
	jne	1f
	cli
	movl	_current_task,%ebp
	movl	RETURN_HOOK(%ebp),%eax
	testl	%eax,%eax
	jz	1f
	movl	%esp,%ebx
	pushl	%ebx
	call	*%eax
	addl	$4,%esp
1:
	RESTORE_REGS
	iret

	.data
	.globl	_intr_nest_count
_intr_nest_count:
	/* This variable counts the number of nested interrupts/exceptions. */
	.long	0
