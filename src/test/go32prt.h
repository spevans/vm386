/* go32prt.h -- Definitions for go32prt.c
   John Harper. */

#ifndef _GO32PRT_H
#define _GO32PRT_H

#include <vmm/types.h>
#include <stdarg.h>

extern bool setup_screen(void);
extern void print(const char *, size_t);

extern void kvsprintf(char *buf, const char *fmt, va_list args);
extern void ksprintf(char *buf, const char *fmt, ...);
extern void kvprintf(const char *fmt, va_list args);
extern void kprintf(const char *fmt, ...);

#endif /* _GO32PRT_H */
