.text

LOAD_SEG	= 0x9000	! 16bit startup code is loaded here

.globl _main
_main:
	int	#3
	cli
	xor	ax,ax
	mov	ss,ax
	mov	sp,#0x7C00
	mov	ax,#0x7C0
	mov	es,ax
	mov	ds,ax
	sti

	cmp	word start16_start,#0
	jne	got_sys_dsk

	mov	si,#not_sys_dsk
	call	show_str
	jmpi	reboot,#0x7c0		! `jmp reboot' is out of range :(

got_sys_dsk:
	mov	dl,boot_dev
	xor	dh,dh
	cmp	dx,#0x80
	jge	got_hd
	call	figure_floppy
	jmp	load_start16
got_hd:	
	call	figure_hard



load_start16:
	mov	ax,#LOAD_SEG
	mov	es,ax			! es:bx = buffer
	xor	bx,bx
	mov	di,start16_count	! di = sectors to load

	! cx = track & sector of first data block in the partition. Now
	! that the fs uses 1024 byte blocks, this *isn't* the first block
	! after the boot block. Use the start16_start variable to find this
	! for real..
	mov	dl,boot_dev
	mov	dh,start16_head
	mov	cx,start16_cylsec

next_track:
	mov	al,sectors	 	! number of sec to read
	mov	ah,#0
	push	cx
	and	cl,#0x3f
	sub	al,cl
	pop	cx

	cmp	ax,di
	jle	do_read
	mov	ax,di
do_read:
	mov	ah,#02
	mov	si,ax
	inc	cl			! sector is 1 based
	int	#0x13
	jnc	read_ok
	mov	bp,#3			! error count
	mov	ah,#0
	int	#0x13			! reset floppy
retry_read:
	mov	ax,si
	int	#0x13
	jnc	read_ok
	dec	bp
	jz	disk_error
	jmp	retry_read
read_ok:
	mov	ax,si
	xor	ah,ah
	sub	di,ax
	jz	all_read
	mov	cl,#9
	shl	ax,cl
	add	bx,ax			! increment buffer pointer
	! no go to next track
	and	cl,#0xC0		! set sector = 0
	add	dh,#1			! next head
	cmp	dh,heads
	jle	next_track
	xor	dh,dh			! head 0
	add	ch,#1			! next track 	
	adc	cl,#0x40		! set top bits of track
	jmp	next_track

all_read:
	jmpi	0, #LOAD_SEG

disk_error:	
	mov	si,#dsk_err_msg
	call	show_str

reboot:
	mov	si,#reboot_msg
	call	show_str
	mov	ah,#0
	int	#0x16
	int	#0x19

show_str:
	mov	ah,#0xe
	mov	bh,#0

next_chr:
	lodsb
	or	al,al
	jz	show_str_end
	int	#0x10
	jmp	next_chr	
show_str_end:
	ret

figure_floppy:
	mov	cylinders,#80
	mov	heads,#2
	mov	ah,#9
	int	#0x13			! reset the floppy

	! now figure out the number of sectors/track for this floppy
	mov	si,#sector_list
get_sectors:
	mov	ax,#0x0201		! load 1 sector
	mov	cl,(si)			! cl = max sector
	xor	dh,dh
	xor	ch,ch			! head 0 track 0
	mov	bx,#0x8000		! buffer to load into
	int	#0x13
	jnc	got_sector_count	! it all worked
	cmp	ah,#4			
	jne	disk_error
	inc	si
	jmp	get_sectors
got_sector_count:
	mov	sectors,cl
	ret

hd_error:
	jmp	disk_error
figure_hard:
	mov	ah,#8
	int	#0x13
	inc	dh
	mov	heads,dh
	mov	dh,cl
	and	dh,0x3F
	mov	sectors,dh
	xchg	ch,cl
	mov	dx,cx
	mov	cl,#6
	shr	dh,cl
	mov	cylinders,dx
	ret
	
not_sys_dsk:	.ascii	"Not a system disk"
	.byte	0x0
dsk_err_msg:	.ascii	"Disk Error"
	.byte	0x0
reboot_msg:	.ascii	" - Press any key to reboot"
	.byte	0xa,0xd,0x0

sector_list:		! list of possible sec/track to try
	.byte	36,18,15,9

! following values filled in by makeimage program
.org	453	
start16_cylsec:		! start cylinder and sector in bios packed format
	.word	0	
start16_head:		! start head 
	.byte	0	
start16_start:		! 0 based LBA start sector of start16
	.long	0
start16_count:		! number of sectors occupied by start16
	.long	0	

kernel_cylsec:		! start cylinder and sector in bios packed format
	.word	0
kernel_head:		! start head
	.byte	0
kernel_start:		! 0 based LBA start sector of kernel 
	.long	0
kernel_sectors:		! number of sectors occupied by kernel
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


! file system info
.org 480
total_blocks:	.long	0
inode_bitmap:	.long	0
inodes:		.long	0
num_inodes:	.long	0
data_bitmap:	.long	0
data:		.long	0
data_size:	.long	0
magic_num:	.word	0
boot_flag:	.word	0xAA55
