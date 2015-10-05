
#ifndef __VMM_SEGMENT_H
#define __VMM_SEGMENT_H

#define USER_CODE	0x08
#define USER_DATA	0x10
#define KERNEL_CODE	0x18
#define	KERNEL_DATA	0x20

#ifndef __ASM__

#include <vmm/types.h>

typedef struct desc_struct {
	unsigned long lo, hi;
} desc_table;

extern desc_table GDT[], IDT[256];


/* Inline functions to move data between the user and kernel segments.
   All assume that %fs contains a selector for the user data segment. */

/* It seems that GAS doesn't always put in the segment prefix (specifically
   `movw %ax,%fs:ADDR' doesn't get one), so I'm encoding the %fs override
   manually. :-( */
#define SEG_FS ".byte 0x64"

static inline void
put_user_byte(u_char val, u_char *addr)
{
    asm (SEG_FS "; movb %b0,%1" : : "iq" (val), "m" (*addr));
}

static inline void
put_user_short(u_short val, u_short *addr)
{
    asm (SEG_FS "; movw %0,%1" : : "ir" (val), "m" (*addr));
}

static inline void
put_user_long(u_long val, u_long *addr)
{
    asm (SEG_FS "; movl %0,%1" : : "ir" (val), "m" (*addr));
}

static inline u_char
get_user_byte(u_char *addr)
{
    u_char res;
    asm (SEG_FS "; movb %1,%0" : "=q" (res) : "m" (*addr));
    return res;
}

static inline u_short
get_user_short(u_short *addr)
{
    u_short res;
    asm (SEG_FS "; movw %1,%0" : "=r" (res) : "m" (*addr));
    return res;
}

static inline u_long
get_user_long(u_long *addr)
{
    u_long res;
    asm (SEG_FS "; movl %1,%0" : "=r" (res) : "m" (*addr));
    return res;
}

static inline void *
memcpy_from_user(void *to, void *from, size_t n)
{
    asm ("cld\n\t"
	 "movl %%edx, %%ecx\n\t"
	 "shrl $2,%%ecx\n\t"
	 "rep ;" SEG_FS "; movsl\n\t"
	 "testb $1,%%dl\n\t"
	 "je 1f\n\t"
	 SEG_FS "; movsb\n"
	 "1:\ttestb $2,%%dl\n\t"
	 "je 2f\n\t"
	 SEG_FS "; movsw\n"
	 "2:"
	 : /* no output */
	 : "d" (n), "D" ((long) to), "S" ((long) from)
	 : "cx", "di", "si", "memory");
    return to;
}

static inline void *
memcpy_to_user(void *to, void *from, size_t n)
{
    asm ("pushw %%es\n\t"
	 "pushw %%fs\n\t"
	 "popw %%es\n\t"
	 "cld\n\t"
	 "movl %%edx, %%ecx\n\t"
	 "shrl $2,%%ecx\n\t"
	 "rep ; movsl\n\t"
	 "testb $1,%%dl\n\t"
	 "je 1f\n\t"
	 "movsb\n"
	 "1:\ttestb $2,%%dl\n\t"
	 "je 2f\n\t"
	 "movsw\n"
	 "2:\tpopw %%es"
	 : /* no output */
	 : "d" (n), "D" ((long) to), "S" ((long) from)
	 : "cx", "di", "si", "memory");
    return to;
}

static inline void *
memcpy_user(void *to, void *from, size_t n)
{
    asm ("pushw %%es\n\t"
	 "pushw %%fs\n\t"
	 "popw %%es\n\t"
	 "cld\n\t"
	 "movl %%edx, %%ecx\n\t"
	 "shrl $2,%%ecx\n\t"
	 "rep; " SEG_FS "; movsl\n\t"
	 "testb $1,%%dl\n\t"
	 "je 1f\n\t"
	 SEG_FS "; movsb\n"
	 "1:\ttestb $2,%%dl\n\t"
	 "je 2f\n\t"
	 SEG_FS "; movsw\n"
	 "2:\tpopw %%es"
	 : /* no output */
	 : "d" (n), "D" ((long) to), "S" ((long) from)
	 : "cx", "di", "si", "memory");
    return to;
}

static inline void *
memset_user(void *s, u_char c, size_t count)
{
    asm ("pushw %%es\n\t"
	 "pushw %%fs\n\t"
	 "popw %%es\n\t"
	 "cld\n\t"
	 "rep\n\t"
	 "stosb\n\t"
	 "popw %%es"
	 : /* no output */
	 : "a" (c), "D" (s), "c" (count)
	 : "cx", "di", "memory");
    return s;
}

static inline void *
memsetw_user(void *s, u_short w, size_t count)
{
    asm ("pushw %%es\n\t"
	 "pushw %%fs\n\t"
	 "popw %%es\n\t"
	 "cld\n\t"
	 "rep\n\t"
	 "stosw\n\t"
	 "popw %%es"
	 : /* no output */
	 : "a" (w), "D" (s), "c" (count)
	 : "cx", "di", "memory");
    return s;
}

static inline void *
memsetl_user(void *s, u_long l, size_t count)
{
    asm ("pushw %%es\n\t"
	 "pushw %%fs\n\t"
	 "popw %%es\n\t"
	 "cld\n\t"
	 "rep\n\t"
	 "stosl\n\t"
	 "popw %%es"
	 : /* no output */
	 : "a" (l), "D" (s), "c" (count)
	 : "cx", "di", "memory");
    return s;
}

#endif /* __ASM__ */
#endif /* __VMM_SEGMENT_H */
