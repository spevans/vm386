.text

.extern _first_esp;
.extern _first_eip;

.globl start_task
start_task:
	movl	$0x20, %ecx	# ss
	push	%ecx
	movl	_first_esp, %ecx
	push	%ecx		# ss:esp is now on stack
	pushf			# so are flags
	orl	$0x200, (%esp)	# set IF flag
	movl	$0x18, %ecx
	push	%ecx		# push cs
	movl	_first_eip, %ecx
	push	%ecx		# push eip

	iret			# should jmp to task	
