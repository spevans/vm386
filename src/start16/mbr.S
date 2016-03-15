;;;         ORG     0x7C00

        .global _main
_main:
        cli
        jmp 0:0x7c00 + here
here:
        xor     ax, ax
        mov     ds, ax
        mov     es, ax
        mov     sp, #0x2000
        sti

        cld
        mov     si, #0x7C00
        mov     di, #0x8000
        mov     cx, #0x100
        rep
        movsb

        jmp     0x40:0x7C00 + here2
here2:
        mov     ax, #0x40
        mov     ds, ax
        push    dx

        mov     ax, #0x201      ; read 1 sector
        mov     cx, #0x0002     ; cylinder 0 sector 2
        mov     dh, #0          ; head 0, dl aready set
        mov     bx, #0x7c00
        int     #0x13
        pop     dx
        jc      load_failed
        jmp     0:0x7c00

load_failed:
        int     0x19

        .org 0x01BE
partition1:     dd      0, 0, 0, 0
partition2:     dd      0, 0, 0, 0
partition3:     dd      0, 0, 0, 0
partition4:     dd      0, 0, 0, 0

boot_flag:      dw      0xAA55
