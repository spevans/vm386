.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

! this is loaded at seg 0x9000, right up high

.globl done

.extern	InitTTY
.extern	CheckSystem
.extern	LoadFiles
.extern	PrintString
.extern cookie_jar
.extern total_mem

entry start
start:
        cli
        mov     ax,cs
        mov     ds,ax
        mov     es,ax
        mov     ss,ax
	mov	sp,#0xFFFE
        sti

	int	#3
        call    InitTTY
	mov	si,#id
	call	PrintString
        call    CheckSystem
        jnc     inreal
	mov	si,#reboot
	call	PrintString
        int     #0x019

inreal:
	call	LoadFiles
	! patch in cookie info
	push	es
	mov	ax,#0x400
	mov	es,ax
	mov	di,#28
	mov	si,#cookie_jar
	mov	cx,#256
	rep	
	movsb
	pop	es
        mov     dx,#0x03f2
        mov     al,#0
        out     dx,al           ; floppy motor off

	mov	si,#proot
	call	PrintString
	push	es
	mov	ax,#0x400
	mov	es,ax
	mov	di,#284
	mov	si,#root_dev
	cld
copy_root:
	lodsb
	stosb
	cmp	al,#0
	jne	copy_root
	pop	es
	mov	si,#done
	call	PrintString
	mov	si,#intoff
	call	PrintString

	mov	al,#0x00		; disable NMI
	mov	dx,#0x70
	out	dx,al
	inc	dx
	in	al,dx
	mov	al,#0xff
	out	0x21,al
	out	0xA1,al
        cli                             ;interrupts off
        mov	si,#done
	call	PrintString
	mov	si,#bodge
	call	PrintString
        call 	debodge 
        mov	si,#done
	call	PrintString

	mov	si,#paging1
	call	PrintString
        push    es

        xor     ecx,ecx

	xor	edx,edx
	mov	dx,total_mem

        shr     edx,#2           ; edx = total number of page table entries
        mov     ebx,edx
	add	ebx,#1023	; round up to next page table boundary
        shr     ebx,#10          ; ebx = total number of page
                                ; directory entries
	xor	ax,ax
        mov     es,ax
        cld
	seg	es
        mov     esi,(0x04004)   ; esi = end of kernel
	add	esi,#4095
	and	esi,#~4095      ; round esi to next 4k boundary
                                ; esi points to location of page table 0

; fill in page directory

        cld
        mov     edi,#0x03000      ; es:di -> page dir
        xor     eax,eax
        mov     ecx,#1024
        rep     
	stosd           	; clear the page directory 

	mov	edi,#0x3000	; es:di -> page dir
	mov	eax,esi		; eax = address of page table 0
	add	eax,#3		; super r/w and present
	seg	es
	mov	(edi+0), eax	; identity mapping for initialisation
	seg	es
	mov	(edi+3968),eax	; kernel page table 0
	add	eax,#0x1000
	seg	es
	mov	(edi+3972),eax	; kernel page table 1

	add	eax,#0x1000	; eax = physical page table 0 (pt 2)	
	mov 	ecx,ebx		; ecx = entries to fill in
next_pd_entry:
	seg	es
	mov	(edi+3976),eax
	add	edi,#4
	add	eax,#0x1000
	loop	next_pd_entry
	

; fill in page table entries


; do the physical map

        mov	edi,esi		; edi -> page table 0
	add	edi,#0x2000	; edi -> page table 2 (physical map 0)
	shr	edi,#4
	mov	es,di
	xor	edi,edi		; es:edi -> page table 2
	mov	eax,#3		; initial entry
	mov	ecx,edx		; ecx = page table entries
fill_phys_map:
	seg	es
	mov	(edi),eax
	add	eax,#0x1000
	add	edi,#4
	loop	fill_phys_map
				
	xor	eax,eax
	mov	ax,es
	shl	eax,#4
	add	edi,eax		; edi -> end of static kernel
	add	edi,#4095
	and	edi,#~4095	; round up to next page
	mov	ax,#0
	mov	es,ax
	seg	es
	mov	(0x4018),edi	; initialising _logical_end_of_kernel

; now do the kernel's map 

	; esi -> page table 0

	shr	esi,#4
	mov	es,si
	xor	esi,esi		; es:esi -> page table 0
	xchg	edi,esi		; es:edi -> page table 0
				; esi -> old edi
	mov	ecx,#0x800-1	; total entries - 1 for NULL reference
	mov	eax,#0
	stosd			; NULL reference
	mov	eax,#0x1003	; page 1

kernel_map:
	stosd
	add	eax,#0x1000
	dec	ecx
	je	end_kernel_map
	! cmp	eax,esi		; if eax >= esi then we've filled
	! jb	kernel_map	; in all the valid entries we need.
        jmp     kernel_map
				; ecx = count to end of table
				; es:edi -> next entry
	xor	eax,eax
	rep
	stosd
end_kernel_map:

	pop	es	
	mov	si,#done
	call	PrintString

	mov	si,#paging2
	call	PrintString
        mov     eax,#0x03000
        mov     cr3,eax
	mov	si,#done
	call	PrintString

/****** end of page setup ********/

        mov	si,#gdt2                    ; set gdt ptr
	call	PrintString
        push    es
        mov     ax,#0x0400
        mov     es,ax
	seg	es
        lgdt    fword ptr (8)
	mov	si,#done
	call	PrintString
	mov	si,#idting
	call    PrintString
	seg	es
        lidt    fword ptr (14)
        pop     es
	mov	si,#done
	call	PrintString

	mov	si,#patchjmp
	call	PrintString
        push    es
        mov     ax,#0x0400
        mov     es,ax
	seg	es
	mov	eax,(0)
        mov     protjmp,eax
        pop     es
	mov	si,#done
	call	PrintString


	mov	si,#goingin
	call	PrintString

        MOV     EAX,CR0
        OR      EAX,#0x080000001          ; SET PROTECTED MODE+paging
        MOV     CR0,EAX
        .byte   0x0EA                    ; JUMP TO 32BIT CODE FLUSHING
protjmp:.long    0                       ; PREFETCH QUEUE


debodge:
        mov al,#0x0ff
        out #0x021,al
        out #0x0a1,al

	mov	bx,#0x02820
	call    set8529vectors
	call    enableA20
	RET

set8529vectors:				; Set new IRQ vector numbers

	mov al,#0x11                      ;  BL - low vector base #
	out #0x20,al                      ;  BH - high vector base #
	call	io_delay
	mov al,bl
	out #0x21,al
	call	io_delay
	mov al,#0x4
	out #0x21,al
	call	io_delay
	mov al,#0x1
	out #0x21,al
	call	io_delay
	mov al,#0x11
	out #0x0a0,al
	call	io_delay
	mov al,bh
	out #0x0a1,al
	call	io_delay
	mov al,#0x2
	out #0x0a1,al
	call	io_delay
	mov al,#0x1
	out #0x0a1,al
	ret


enableA20:      		; Enable gate A20
        call    empty_8042
        mov     al,#0xD1                ! command write
        out     #0x64,al
        call    empty_8042
        mov     al,#0xDF                ! A20 on
        out     #0x60,al
        call    empty_8042
	ret

empty_8042:
        call    io_delay
        in      al,#0x64        ! 8042 status port
        test    al,#1           ! output buffer?
        jz      no_output
        call    io_delay
        in      al,#0x60        ! read it
        jmp     empty_8042
no_output:
        test    al,#2           ! is input buffer full?
        jnz     empty_8042      ! yes - loop
        ret


io_delay:
	jmp	end_io_delay
end_io_delay:
	ret

	
id:		.ascii	'This is start16'
		db	0x0a, 0x0d, 0x00
reboot:		.ascii	'Reboot System Now'
		db	0x0a, 0x0d, 0x00
oksofar:	.ascii	'Ok so far'
		db	0x0a, 0x0d, 0x00

gdt1:		.ascii	'Moving GDT to new location...'
		db	0x00
gdt2:		.ascii	'Loading Global Descriptor Table Pointer...'
		db	0x00
idting:		.ascii	'Loading Interrupt Descriptor Table Pointer...'
		db	0x00
paging1:	.ascii	'Setting Up Page Tables...'
		db	0x00
paging2:	.ascii	'Loading Page Table Base Register...'
		db	0x00
nullidt:	.ascii	'Loading NULL Interrupt Descriptor Table Pointer...'
		db	0x00
intoff:		.ascii	'Turning off interrupts...'
		db	0x00
patchjmp:	.ascii	'Patching in 32bit jmp address...'
		db	0x00
done:		.ascii	' done'
		db	0x0a, 0x0d, 0x00
goingin:	.ascii	'Entering Protected Mode'
		db	0x0a, 0x0d, 0x00
bodge:		.ascii	'Debodging system...'
		db	0x00
proot:		.ascii	'Patching in root device...'
		db	0x00
root_dev:	.ascii	"hda1:"
		db	0x00	

.text
endtext:
.data
enddata:
.bss
endbss:
