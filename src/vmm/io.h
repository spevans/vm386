/* io.h -- inline assembly functions to allow I/O instructions in C code.
   John Harper. */

#ifndef _VMM_IO_H
#define _VMM_IO_H

#include <vmm/types.h>


/* Standard I/O functions. */

static inline u_char
inb(u_short port)
{
    u_char res;
    asm volatile ("inb %w1,%b0" : "=a" (res) : "d" (port));
    return res;
}

static inline void
outb(u_char value, u_short port)
{
    asm volatile ("outb %b0,%w1" : : "a" (value), "d" (port));
}

static inline u_short
inw(u_short port)
{
    u_short res;
    asm volatile ("inw %w1,%w0" : "=a" (res) : "d" (port));
    return res;
}

static inline void
outw(u_short value, u_short port)
{
    asm volatile ("outw %w0,%w1" : : "a" (value), "d" (port));
}

static inline u_long
inl(u_short port)
{
    u_long res;
    asm volatile ("inl %w1,%l0" : "=a" (res) : "d" (port));
    return res;
}

static inline void
outl(u_long value, u_short port)
{
    asm volatile ("outl %l0,%w1" : : "a" (value), "d" (port));
}


/* I/O functions which pause after accessing the port. */

static inline void
io_pause(void)
{
#if 0
    asm volatile ("jmp 1f\n1:\tjmp 1f\n1:");
#else
    asm volatile ("outb %al,$0x84");
#endif
}

static inline void
outb_p(u_char value, u_short port)
{
    outb(value, port);
    io_pause();
}

static inline u_char
inb_p(u_short port)
{
    u_char c = inb(port);
    io_pause();
    return c;
}

static inline void
outw_p(u_short value, u_short port)
{
    outw(value, port);
    io_pause();
}

static inline u_short
inw_p(u_short port)
{
    u_short w = inw(port);
    io_pause();
    return w;
}

static inline void
outl_p(u_long value, u_short port)
{
    outl(value, port);
    io_pause();
}

static inline u_long
inl_p(u_short port)
{
    u_long l = inl(port);
    io_pause();
    return l;
}


/* Block I/O functions. */

static inline void
insb(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; insb"
                  : "+D" (buf), "+c" (count)
                  : "d" (port)
                  : "memory");
}

static inline void
outsb(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; outsb"
                  : "+S" (buf), "+c" (count)
                  : "d" (port)
                  : "memory" );
}

static inline void
insw(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; insw"
                  : "+D" (buf), "+c" (count)
                  : "d" (port)
                  : "memory");
}

static inline void
outsw(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; outsw"
                  : "+S" (buf), "+c" (count)
                  : "d" (port)
                  : "memory");
}

static inline void
insl(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; insl"
                  : "+D" (buf), "+c" (count)
                  : "d" (port)
                  : "memory");
}

static inline void
outsl(void *buf, int count, u_short port)
{
    asm volatile ("cld\n\t"
                  "rep ; outsl"
                  : "+S" (buf), "+c" (count)
                  : "d" (port)
                  : "memory");
}


/* The following probably don't belong here, but it'll do for now... */

#define cli() do { asm volatile ("cli" : : : "memory"); } while(0)
#define sti() do { asm volatile ("sti" : : : "memory"); } while(0)
#define hlt() do { asm volatile ("hlt" : : : "memory"); } while(0)
#define nop() do { asm volatile ("nop"); } while(0)

#define save_flags(x) \
    do { asm volatile ("pushfl ; popl %0" : "=r" (x) : : "memory"); } while(0)
#define load_flags(x) \
    do { asm volatile ("pushl %0 ; popfl" : : "r" (x) : "memory"); } while(0)

#endif /* _VMM_IO_H */
