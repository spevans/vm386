/* io.h -- inline assembly functions to allow I/O instructions in C code.
   John Harper. */

#ifndef _VMM_IO_H
#define _VMM_IO_H

#include <vmm/types.h>


/* Standard I/O functions. */

extern inline u_char
inb(u_short port)
{
    u_char res;
    asm volatile ("inb %w1,%b0" : "=a" (res) : "d" (port));
    return res;
}

extern inline void
outb(u_char value, u_short port)
{
    asm volatile ("outb %b0,%w1" : : "a" (value), "d" (port));
}

extern inline u_short
inw(u_short port)
{
    u_short res;
    asm volatile ("inw %w1,%w0" : "=a" (res) : "d" (port));
    return res;
}

extern inline void
outw(u_short value, u_short port)
{
    asm volatile ("outw %w0,%w1" : : "a" (value), "d" (port));
}

extern inline u_long
inl(u_short port)
{
    u_long res;
    asm volatile ("inl %w1,%l0" : "=a" (res) : "d" (port));
    return res;
}

extern inline void
outl(u_long value, u_short port)
{
    asm volatile ("outl %l0,%w1" : : "a" (value), "d" (port));
}


/* I/O functions which pause after accessing the port. */

extern inline void
io_pause(void)
{
#if 0
    asm volatile ("jmp 1f\n1:\tjmp 1f\n1:");
#else
    asm volatile ("outb %al,$0x84");
#endif
}

extern inline void
outb_p(u_char value, u_short port)
{
    outb(value, port);
    io_pause();
}

extern inline u_char
inb_p(u_short port)
{
    u_char c = inb(port);
    io_pause();
    return c;
}

extern inline void
outw_p(u_short value, u_short port)
{
    outw(value, port);
    io_pause();
}

extern inline u_short
inw_p(u_short port)
{
    u_short w = inw(port);
    io_pause();
    return w;
}

extern inline void
outl_p(u_long value, u_short port)
{
    outl(value, port);
    io_pause();
}

extern inline u_long
inl_p(u_short port)
{
    u_long l = inl(port);
    io_pause();
    return l;
}


/* Block I/O functions. */

extern inline void
insb(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; insb"
		  : /* no outputs */
		  : "d" (port), "D" (buf), "c" (count)
		  : "edi", "ecx", "memory");
}

extern inline void
outsb(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; outsb"
		  : /* no outputs */
		  : "d" (port), "S" (buf), "c" (count)
		  : "esi", "ecx", "memory");
}

extern inline void
insw(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; insw"
		  : /* no outputs */
		  : "d" (port), "D" (buf), "c" (count)
		  : "edi", "ecx", "memory");
}

extern inline void
outsw(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; outsw"
		  : /* no outputs */
		  : "d" (port), "S" (buf), "c" (count)
		  : "esi", "ecx", "memory");
}

extern inline void
insl(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; insl"
		  : /* no outputs */
		  : "d" (port), "D" (buf), "c" (count)
		  : "edi", "ecx", "memory");
}

extern inline void
outsl(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
		  "rep ; outsl"
		  : /* no outputs */
		  : "d" (port), "S" (buf), "c" (count)
		  : "esi", "ecx", "memory");
}


/* The following probably don't belong here, but it'll do for now... */

#define cli() asm volatile ("cli" : : : "memory")
#define sti() asm volatile ("sti" : : : "memory")
#define hlt() asm volatile ("hlt" : : : "memory")
#define nop() asm volatile ("nop")

#define save_flags(x) \
    asm volatile ("pushfl ; popl %0" : "=r" (x) : : "memory")
#define load_flags(x) \
    asm volatile ("pushl %0 ; popfl" : : "r" (x) : "memory")

#endif /* _VMM_IO_H */
