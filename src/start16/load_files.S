.text

DEST_SEG	= 0x400		! where the kernel sits	
LOAD_SEG	= 0x8000	! where it is loaded

!SECS_PER_TRACK	= 9
!HEADS		= 2

.extern PrintString
.extern done

.globl LoadFiles
LoadFiles:
	mov	si,#loading_kernel
	call	PrintString
	! get disk and file info
	push	ds
	mov	ax,#0x7c0
	mov	ds,ax
	mov	si,#464
	mov	di,#kernel_cylsec
	mov	cx,#16
	rep
	movsb
	pop	ds	

	mov	dl,boot_dev
	xor	dh,dh
	cmp	dx,#0x80
	jge	not_floppy
	mov	ah,#9
	int	#0x13			! reset the floppy

not_floppy:
	mov	esi,kernel_start
	mov	edi,kernel_count
	mov	ax,#LOAD_SEG
	mov	es,ax
next_load:
	xor	bx,bx			! es:bx -> load buffer
	push	edi
	call	load_track
	and	eax,#0xFFFF
	push	eax
	call	do_block_copy
	call	do_load_mesg
	pop	eax
	pop	edi
	sub	edi,eax
	jz	all_loaded
	add	esi,eax
	jmp	next_load

all_loaded:
	mov	si,#done
	call	PrintString
	ret


! load_sectors loads as many sectors from a track as possible
! maximum sectors loaded per call is 64 
! es: -> buffer_seg  dl = drive  esi = logical sector edi = sectors to load
! return eax = sectors loaded 

load_track:
	call	log_to_phys
	
	xor	eax,eax
	and	ebx,#0xff		! ebx = start sector
	mov	al,sectors
	sub	eax,ebx			! al = sectors in track to load
	cmp	eax,edi
	jle	sec_count_ok
	mov	eax,edi
	
sec_count_ok:
	inc	cl			! sector is 1 based
	mov	ah,#02
	mov	di,ax
	xor	bx,bx			! es:bx -> buffer
	int	#0x13
	jnc	read_ok
	mov	bp,#3			! error count
	mov	ah,#0
	int	#0x13			! reset floppy
retry_read:
	mov	ax,di
	int	#0x13
	jnc	read_ok
	dec	bp
	jz	disk_error
	jmp	retry_read
read_ok:
	mov	ax,di
	and	eax,#0xFF
	ret				! eax = sectors read


! disk_error displays an error message, waits for a key then reboots
disk_error:	
	mov	si,#mesg
	call	PrintString
	mov	ah,#0
	int	#0x16
	int	#0x19

! do_block_copy copies ax sectors from LOAD_SEG:0 to dest_seg:0
! dest_seg is updated

do_block_copy:
	push	cx
	push	ds
	push	es
	push	di
	push	si
	xor	di,di
	xor	si,si
	shl	ax,#7			! number of bytes/4 loaded
	mov	cx,dest_seg
	mov	es,cx
	mov	cx,#LOAD_SEG
	mov	ds,cx
	mov	cx,ax
	rep
	movsd
	shr	ax,#2			! number of paragraphs copied
	pop	si
	pop	di
	pop	es
	pop	ds
	pop	cx
	add	dest_seg,ax
	ret

! do_load_mesg displays a symbol to show when a track has been loaded

do_load_mesg:
	push	si
	push	bx
	mov	bx,next_mesg
	mov	si,track_mesg(bx)
	call	PrintString
	add	bx,#2
	and	bx,#7
	mov	next_mesg,bx
	pop	bx
	pop	si
	ret

! convert logical sector in esi to 
! cl = sector ch = track dh = head top 2 bits of cl = bits 8,9 of track
! dl = drive

log_to_phys:
        mov     eax,esi
        xor     edx,edx
        xor     ecx,ecx
        mov     cl,sectors
        div     ecx
        mov     bl,dl                   ! edx = remainder, dl = sector
        xor     edx,edx
        xor     ecx,ecx
        mov     cl,heads
        div     ecx
        ! bl = sector ax = track dl = head, now combine track and sector
        mov     dh,dl                   ! dh = head
        mov     dl,boot_dev             ! dl = drive
        mov     ch,al
        mov     bh,ah
        shl     bh,#6
        or      bh,bl
        mov     cl,bh   ! cl = sector + bits 8,9 track
	ret

mesg:	.ascii	"Disk Error - Press any key to reboot"
	.byte	0

load1:	.ascii	"|"
	.byte	8,0
load2:	.ascii	"/"
	.byte	8,0
load3:	.ascii	"-"
	.byte	8,0
load4:	.ascii	"\\"
	.byte	8,0

track_mesg:	.word	load1, load2, load3, load4
next_mesg:	.word	0


loading_kernel:
	.ascii	"Loading Kernel..."
	.byte	0


! grabbed from the bootsector


kernel_cylsec:		! start cylinder and sector in bios packed format
	.word	0
kernel_head:		! start head
	.byte	0
kernel_start:		! 0 based LBA start sector of kernel 
	.long	0
kernel_count:		! number of sectors occupied by kernel
	.long	0

boot_dev:
	.byte	0	! 0+ = floppies 80h+=fixed etc

! disk info, used for both fixed and floppy
cylinders:
	.word	0
heads:
	.byte	0
sectors:
	.byte	0

dest_seg:
	.word	DEST_SEG	
