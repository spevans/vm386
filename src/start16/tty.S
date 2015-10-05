#define SCR_WIDTH       80
#define SCR_HEIGHT      25
#define MAX_COLS        SCR_WIDTH-1
#define MAX_ROWS        SCR_HEIGHT-1
#define TABWIDTH        8

.text

ScrBase:        .long   0xB8000000
Row:            .byte   0
Col:            .byte   0
str1:           .ascii	"TTY Driver Initialised"
                db      0x0a, 0x0d
                .ascii  "----------------------"
                db      0x0a, 0x0d, 0x0d, 0x00

.globl SetCursorPos
.globl ScrollUp
.globl SetCursorType
.globl PrintString
.globl Cls
.globl InitTTY
.globl HexDump
.globl printLong

! dl = x dh = y
SetCursorPos:

        push    bx
        push    cx
        cmp     dl,#MAX_COLS
        jg      scperror
        cmp     dh,#MAX_ROWS
        jg      scperror

        mov     Col,dl
        mov     Row,dh
        mov     cl,#80
        mov     al,dh
        mul     cl
        add     al,dl
        adc     ah,#0
        mov     bx,ax

        mov     dx,#0x3d4
        mov     al,#0xe
        out     dx,al
        inc     dx
        mov     al,bh
        out     dx,al

        mov     dx,#0x3d4
        mov     al,#0xf
        out     dx,al
        inc     dx
        mov     al,bl
        out     dx,al

scperror:
        pop     cx
        pop     bx
        ret


ScrollUp:

        push    ds
        push    es
        push    di
        push    si
        push    cx
        les     di,ScrBase
        lds     si,ScrBase
        add     si,#160         ! SCR_WIDTH*2
        mov     cx,#1920        ! SCR_WIDTH * (SCR_HEIGHT-1)
        cld
        rep    
        movsw
        mov     cx,#SCR_WIDTH
        mov     ax,#0x0720
        rep
        stosw

        pop     cx
        pop     si
        pop     di
        pop     es
        pop     ds
        push    dx
        mov     dl,Col
        mov     dh,Row
        call    SetCursorPos
        pop     dx
        ret


! ch = Top cl = Bottom
SetCursorType:

        mov dx,#0x3d4
        mov al,#0xa
        out dx,al
        inc dx
        mov al,cl
        out dx,al

        mov dx,#0x3d4
        mov al,#0xb
        out dx,al
        inc dx
        mov al,ch
        out dx,al
        ret


! ds:si -> string
PrintString:
	push	ax
	push	bx
	push	cx
	push	dx

nextchr:
        mov     al,(si)
        or      al,al
        jz      strend
        inc     si

	cmp	al,#8
	jne	notbs
	mov	cl,Col
	cmp	cl,#0
	je	nextchr
	dec	cl
	mov	Col,cl
	jmp	nextchr

notbs:
        cmp     al,#13
        jne     notcr
        mov     byte Col,#0
        call	testxy
	jmp	nextchr

notcr:
        cmp     al,#10
        jne     notnl
        mov     byte Col,#0
        inc     byte Row
        call    testxy
	jmp	nextchr

notnl:
        cmp     al,#9
        jne     nottab
        mov     al,Col
        cbw
        mov     cl,#TABWIDTH
        div     cl
        add     Col,ah
        call    testxy
	jmp	nextchr
nottab:
        call    print_char
        jmp     nextchr

        strend:
        mov     dl,Col
        mov     dh,Row
        call    SetCursorPos
	pop	dx
	pop	cx
	pop	bx
	pop	ax
        ret

        ! prints character in AL to screen at current position
        ! clobbers ah
print_char:
        push    bx
        push    cx
        push    es
        push    di
        les     di,ScrBase
        mov     ch,#7
        mov     cl,al
        mov     al,Row
        cbw
        mov     bl,#SCR_WIDTH
        mul     bl
        add     al,Col
        adc     ah,#0
        shl     ax,#1
        add     di,ax
	seg	es
        mov     (di),cx
        inc     byte Col
	call	testxy
        pop     di
        pop     es
        pop     cx
        pop     bx
	ret

testxy:
        mov     al,Col
        cmp     al,#SCR_WIDTH
        jl      colok
        mov     byte Col,#0
        inc     byte Row

colok:

        mov     al,Row
        cmp     al,#SCR_HEIGHT
        jl      rowok
        call    ScrollUp
        mov     byte Row,#MAX_ROWS-1

rowok:
	ret


Cls:
        push    di
        push    es
        les     di,ScrBase
        mov     cx,#SCR_WIDTH * SCR_HEIGHT
        mov     ax,#0x0720
        cld
        rep
	stosw
        mov     byte Row,#0
        mov     byte Col,#0
        xor     dx,dx
        call    SetCursorPos

        pop     es
        pop     di
        ret


InitTTY:
        mov     ax,#3
        int     #0x10
        mov     byte Row,#0
        mov     byte Col,#0
        xor     dx,dx
        call    SetCursorPos
	mov	si,#str1
	call	PrintString
        ret

! Dump CL bytes from ES:SI
HexDump:
        push    bx
        cmp     cl,#0
        je      HexDumpEnd

next_line:
        mov     byte Col,#0     ! reset to a newline
        inc     byte Row
        call    testxy
        mov     ch,#0           ! number of bytes output perline
        mov     bx,es
        call    printWord       ! print seg:offset
        mov     al,#58          ! ':'
        call    print_char
        mov     bx,si
        call    printWord
        mov     al,#32
        call    print_char

next_byte:
        seg     es
        mov     bl,(si)
        call    printByte
        dec     cl
        cmp     cl,#0
        je      HexDumpEnd
        inc     si
        inc     ch
        cmp     ch,#16
        je      next_line
        mov     al,#32          ; space
        call    print_char
        jmp     next_byte

HexDumpEnd:
        mov     byte Col,#0     ! reset to a newline
        inc     byte Row
        call    testxy
        push    dx
        mov     dl,Col
        mov     dh,Row
        call    SetCursorPos
        pop     dx
        pop     bx
        ret

! print the hex digit in low 4 bits of AL to the screen
PrintNibble:
        and     al,#0xf
        add     al,#48
        cmp     al,#58          ! is AL  A-F?
        jl      less_than_a
        add     al,#7
less_than_a:
        call    print_char
        ret

! print the byte in BL to the screen
printByte:
        mov     al,bl
        shr     al,#4
        call    PrintNibble
        mov     al,bl
        call    PrintNibble
        ret

! print the word in BX to the screen
printWord:
        mov     al,bh
        shr     al,#4
        call    PrintNibble
        mov     al,bh
        call    PrintNibble
        call    printByte
        ret

! print the long in EBX to the screen
printLong:
        mov     edx,ebx
        shr     ebx,#16
        call    printWord
        mov     bx,dx
        call    printWord
        ret
