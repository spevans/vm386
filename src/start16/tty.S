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

! dl = x dh = y
SetCursorPos:

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
        ret


ScrollUp:

        push    ds
        push    es
        push    di
        push    si

        les     di,ScrBase
        lds     si,ScrBase
        add     si,#MAX_COLS * 2
        mov     cx,#SCR_WIDTH * SCR_HEIGHT-1 * 2
        cld
        rep    
	movsb
        mov     cx,#SCR_WIDTH
        mov     ax,#0x0720
        rep
	stosw

        pop     si
        pop     di
        pop     es
        pop     ds
        mov     dl,Col
        mov     dh,Row
        call    SetCursorPos
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
        push    es
        push    di
	push	ax
	push	bx
	push	cx
	push	dx

nextchr:
        les     di,ScrBase
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
	jmp	nextchr

strend:
        mov     dl,Col
        mov     dh,Row
        call    SetCursorPos
	pop	dx
	pop	cx
	pop	bx
	pop	ax
        pop     di
        pop     es
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
