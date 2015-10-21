#define	__ASM__

#include <vmm/tasks.h>

.text	
.globl _end_of_kernel
.globl _kernel_page_dir
.globl _main
.globl _IDT, _GDT
.globl _logical_top_of_kernel
.globl _cookie
.globl _root_dev


_main:					# offsets 
	.word	start_of_kernel		# 0   0x00
	.word	0x18			# 2   0x02
_end_of_kernel:
	.long	__end			# 4   0x04
		
GDTInfo:
	.word	(GDTEnd-_GDT)-1		# 8   0x08
	.long	0xF8000000 + _GDT	# 10  0x0a

IDTInfo:
	.word	(IDTEnd-_IDT)-1		# 14  0x0e
	.long	0xF8000000 + _IDT	# 16  0x10

_kernel_page_dir:
	.long	0x0			# 20  0x14

_logical_top_of_kernel:
	.long	0x100000		# 24  0x18

_cookie:	
	.space	256			# 28  0x1c
_root_dev:
	.space	40			# 284 0x11c

start_of_kernel:
	mov	$0x20,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	mov	%ax,%ss
	movl	$0x3000,%esp

	movl	%cr3,%edi
	movl	%edi, _kernel_page_dir

	/* Reload all idt/gdt stuff. Last time it was loaded was
	   in start16.S, i.e. real mode, unfortunately the 386
	   ignores the high 8 bits of the base in real mode, so
	   load it again to get it right. */
	lgdt	GDTInfo
	lidt	IDTInfo
	movl	$0, %eax
	lldt	%eax	/* paranoia */

	/* Now remove the physical==linear==logical mapping.. */
	movl	$0,(%edi)
	/* ..and flush the TLB. */
	movl	%edi,%cr3

	/* Clear BSS first so that there are no surprises... */
        xorl %eax,%eax
        movl $__edata,%edi
        movl $__end,%ecx
        subl %edi,%ecx
        cld
        rep
        stosb

	call	setup_idt
	cld
	call	_main_kernel
here2:
	jmp	here2

setup_idt:
        lea ignore_int,%edx
        movl $(0x18 << 16),%eax
        movw %dx,%ax            /* selector = 0x0010 = cs */
        movw $0x8E00,%dx        /* interrupt gate - dpl=0, present */

        lea _IDT,%edi
        mov $256,%ecx
rp_sidt:
        movl %eax,(%edi)
        movl %edx,4(%edi)
        addl $8,%edi
        dec %ecx
        jne rp_sidt
        ret


ignore_int:
        cld
        pushl %eax
        pushl %ecx
        pushl %edx
        push %ds
        push %es
        push %fs
        movl $0x20,%eax
        mov %ax,%ds
        mov %ax,%es
        mov %ax,%fs
        /*pushl $unexp_int
        call _ttyout */
        popl %eax
        pop %fs
        pop %es
        pop %ds
        popl %edx
        popl %ecx
        popl %eax
	iret

.globl _get_ss
_get_ss:
	xorl	%eax,%eax
	movl	%ss,%ax
	ret

.globl _get_esp
_get_esp:
	movl	%esp,%eax
	ret

.data
_IDT:
	.fill	256,8,0
IDTEnd:

.align 4
_GDT:
	.quad	0x0000000000000000	# null desc
	.quad	0x00CFFA0000007FFF	# 4G-128M user code @ 0x0
	.quad	0x00CFF20000007FFF	# 4G-128M user data @ 0x0
	.quad	0xF8C09A0000007FFF	# 128M kernel code @ 0xf8000000
	.quad	0xF8C0920000007FFF	# 128M kernel data @ 0xf8000000
	.fill	MAX_TASKS, 8, 0		#space for 16 tasks
GDTEnd:

mesg1:	.asciz	"_main_kernel has finished"
unexp_int:
	.asciz	"unexpected interrupt!"

