.text

str1:	.ascii	"Detecting CPU Type: "
	db	0x00
str2:	.ascii	"386+"
	db	0x0d, 0x0a, 0x00
str3:	.ascii	"Checking CPU Mode: "
	db	0x00
str4:	.ascii	"Protected Mode"
	db	0x0d, 0x0a, 0x00
str5:	.ascii	"Real Mode"
	db	0x0d, 0x0a, 0x00
str6:	.ascii	"I need a 386 at least!"
	db	0x0d, 0x0a, 0x00
str7:	.ascii	"I want a real mode 386!"
	db	0x0d, 0x0a, 0x00
str8:	.ascii	"so far, so good"
	db	0x0d, 0x0a, 0x00


.globl cookie_jar
cookie_jar:
.globl total_mem
total_mem:	.word	0
! filled in by cpuid.S
.globl cpu_type
  cpu_type:       .byte   0
.globl fpu_type
  fpu_type:       .byte   0
.globl id_flag
  id_flag:        .byte   0
.globl vendor_id 
  vendor_id:      .space  12
.globl model
  model:          .byte   0
.globl stepping
  stepping:       .byte   0
.globl intel_proc
  intel_proc:     .byte   0
.globl feature_flags
  feature_flags:  .word   0,0
com_ports:	.word	0,0,0,0
lpr_port:	.word	0,0,0,0
hdcount:	.byte	0
hdinfo:		.space	32
fdinfo:		.byte	0,0

.extern	PrintString
.extern	cpuid

.globl	CheckSystem

Got386:
	mov	si,#str1
	call	PrintString
	mov	si,#str2
	call	PrintString
        clc
        ret

RealMode:
	mov	si,#str3
	call	PrintString	
        smsw    ax
        test    ax,#1
        jnz     real
	mov	si,#str5
	call	PrintString
        clc
        ret

real:
	mov	si,#str4
	call	PrintString
        stc
        ret


CheckSystem:
        call   	cpuid 
	cmpb	cpu_type,#3
	jge	ok386
	mov	si,#str6
	call	PrintString
        stc
        ret

ok386:
        call    RealMode
        jnc     inreal
	mov	si,#str7
	call	PrintString
        stc
        ret

inreal:
	call	GetMemory
	call	GetLprComPorts
	call	GetHdinfo
	call	GetFdinfo
	mov	si,#str8
	call	PrintString
        clc
        ret

GetMemory:
	mov	al,#0x17
	out	#0x70,al
	in	al,#0x71
	mov	dl,al
	mov	al,#0x18
	out	#0x70,al
	in	al,#0x71
	mov	dh,al
	add	dx,#1024
	mov	total_mem,dx
	ret

GetLprComPorts:
	push	ds	
	mov	ax,#0x40
	mov	ds,ax
	xor	si,si
	mov	di,#com_ports
	mov	cx,#4
	rep
	movsd
	pop	ds	
	ret

GetHdinfo:
	mov	al,#0x12
	out	#0x70,al
	in	al,#0x71		! al = cmos hd types 
	mov	ah,al
	and	al,#0x0f
	jz	no_drive1
	inc	hdcount
no_drive1:
	and	ah,#0xf0
	jz	no_drive2
	inc	hdcount

no_drive2:	
	push	es
	push	ds
	xor	ax,ax
	mov	es,ax

	seg	es
	lds	si,[4* 0x41]
	mov	di,#hdinfo
	push	cs
	pop	es
	mov	cx,#16
	rep
	movsb
	mov	es,ax
	seg	es
	lds	si,[4* 0x46]
	push	cs
	pop	es
	mov	cx,#16
	rep	
	movsb
	pop	ds
	pop	es
	ret

GetFdinfo:
	mov	al,#0x10	! get floppy types from CMOS
	out	#0x70,al
	in	al,#0x71
	mov	ah,al		
	shr	ah,#4
	and	ax,#0x0f0f
	xchg	al,ah
	mov	fdinfo,ax
	ret
