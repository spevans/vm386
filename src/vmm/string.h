/* string.h -- definitions of standard string functions
   John Harper. */

#ifndef _VMM_STRING_H
#define _VMM_STRING_H

#ifdef i386

#include <vmm/types.h>

/* Unfortunately Linux doesn't define stpcpy() so here's my attempt.. */
static inline char *
stpcpy(char *dest, const char *src)
{
    /* Need the `volatile' keyword so gcc doesn't assume it's free
       of side-effects (because of the output operand). */
    asm volatile ("cld\n\t"
		  "1:\tlodsb\n\t"
		  "stosb\n\t"
		  "testb %%al,%%al\n\t"
		  "jne 1b\n\t"
		  "decl %%edi"
		  : "=D" (dest)
		  : "S" (src), "0" (dest)
		  : "ax", "memory");
    return dest;
}

/* As the following comment gives away, the rest of this file was
   appropriated from Linux; so it may even work 8-)  */


/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		Copyright (C) 1991, 1992 Linus Torvalds
 */
 
static inline char *
strcpy(char *dest, const char *src)
{
        int d0, d1, d2;
        asm volatile("cld\n\t"
                     "1:\tlodsb\n\t"
                     "stosb\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b"
                     : "=&S" (d0), "=&D" (d1), "=&a" (d2)
                     : "0" (src), "1" (dest) : "memory");
        return dest;
}

static inline char *
strncpy(char *dest, const char *src, size_t count)
{
        int d0, d1, d2, d3;
        asm volatile("cld\n\t"
                     "1:\tdecl %2\n\t"
                     "js 2f\n\t"
                     "lodsb\n\t"
                     "stosb\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b\n\t"
                     "rep\n\t"
                     "stosb\n"
                     "2:"
                     : "=&S" (d0), "=&D" (d1), "=&c" (d2), "=&a" (d3)
                     : "0" (src), "1" (dest), "2" (count) : "memory");
        return dest;
}

static inline char *
strcat(char *dest, const char *src)
{
        int d0, d1, d2, d3;
        asm volatile("cld\n\t"
                     "repne\n\t"
                     "scasb\n\t"
                     "decl %1\n"
                     "1:\tlodsb\n\t"
                     "stosb\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b"
                     : "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
                     : "0" (src), "1" (dest), "2" (0), "3" (0xffffffffu) : "memory");
        return dest;
}

static inline char *
strncat(char *dest, const char *src, size_t count)
{
        int d0, d1, d2, d3;
        asm volatile("cld\n\t"
                     "repne\n\t"
                     "scasb\n\t"
                     "decl %1\n\t"
                     "movl %8,%3\n"
                     "1:\tdecl %3\n\t"
                     "js 2f\n\t"
                     "lodsb\n\t"
                     "stosb\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b\n"
                     "2:\txorl %2,%2\n\t"
                     "stosb"
                     : "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
                     : "0" (src), "1" (dest), "2" (0), "3" (0xffffffffu), "g" (count)
                     : "memory");
        return dest;
}

static inline int
strcmp(const char *cs, const char *ct)
{
        int d0, d1;
        int res;
        asm volatile("cld\n\t"
                     "1:\tlodsb\n\t"
                     "scasb\n\t"
                     "jne 2f\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b\n\t"
                     "xorl %%eax,%%eax\n\t"
                     "jmp 3f\n"
                     "2:\tsbbl %%eax,%%eax\n\t"
                     "orb $1,%%al\n"
                     "3:"
                     : "=a" (res), "=&S" (d0), "=&D" (d1)
                     : "1" (cs), "2" (ct)
                     : "memory");
        return res;
}

static inline int
strncmp(const char *cs, const char *ct, size_t count)
{
        int res;
        int d0, d1, d2;
        asm volatile("cld\n\t"
                     "1:\tdecl %3\n\t"
                     "js 2f\n\t"
                     "lodsb\n\t"
                     "scasb\n\t"
                     "jne 3f\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b\n"
                     "2:\txorl %%eax,%%eax\n\t"
                     "jmp 4f\n"
                     "3:\tsbbl %%eax,%%eax\n\t"
                     "orb $1,%%al\n"
                     "4:"
                     : "=a" (res), "=&S" (d0), "=&D" (d1), "=&c" (d2)
                     : "1" (cs), "2" (ct), "3" (count)
                     : "memory");
        return res;
}

static inline char *
strchr(const char *s, char c)
{
        int d0;
        char *res;
        asm volatile("cld\n\t"
                     "movb %%al,%%ah\n"
                     "1:\tlodsb\n\t"
                     "cmpb %%ah,%%al\n\t"
                     "je 2f\n\t"
                     "testb %%al,%%al\n\t"
                     "jne 1b\n\t"
                     "movl $1,%1\n"
                     "2:\tmovl %1,%0\n\t"
                     "decl %0"
                     : "=a" (res), "=&S" (d0)
                     : "1" (s), "0" (c)
                     : "memory");
        return res;
}


static inline char *
strpbrk(const char *cs, const char *ct)
{
    register char *__res;
    asm ("cld\n\t"
	 "movl %4,%%edi\n\t"
	 "repne\n\t"
	 "scasb\n\t"
	 "notl %%ecx\n\t"
	 "decl %%ecx\n\t"
	 "movl %%ecx,%%edx\n"
	 "1:\tlodsb\n\t"
	 "testb %%al,%%al\n\t"
	 "je 2f\n\t"
	 "movl %4,%%edi\n\t"
	 "movl %%edx,%%ecx\n\t"
	 "repne\n\t"
	 "scasb\n\t"
	 "jne 1b\n\t"
	 "decl %0\n\t"
	 "jmp 3f\n"
	 "2:\txorl %0,%0\n"
	 "3:"
	 : "=S" (__res)
	 : "a" (0), "c" (0xffffffff), "0" (cs), "g" (ct)
	 : "ax", "cx", "dx", "di");
    return __res;
}

static inline char *
strstr(const char *cs, const char *ct)
{
    register char *__res;
    asm ("cld\n\t"
	 "movl %4,%%edi\n\t"
	 "repne\n\t"
	 "scasb\n\t"
	 "notl %%ecx\n\t"
	 /* NOTE! This also sets Z if searchstring='' */
	 "decl %%ecx\n\t"
	 "movl %%ecx,%%edx\n"
	 "1:\tmovl %4,%%edi\n\t"
	 "movl %%esi,%%eax\n\t"
	 "movl %%edx,%%ecx\n\t"
	 "repe\n\t"
	 "cmpsb\n\t"
	 /* also works for empty string, see above */
	 "je 2f\n\t"
	 "xchgl %%eax,%%esi\n\t"
	 "incl %%esi\n\t"
	 "cmpb $0,-1(%%eax)\n\t"
	 "jne 1b\n\t"
	 "xorl %%eax,%%eax\n\t"
	 "2:"
	 : "=a" (__res)
	 : "0" (0), "c" (0xffffffff), "S" (cs), "g" (ct)
	 : "cx", "dx", "di", "si");
    return __res;
}

static inline size_t
strlen(const char *s)
{
        int d0;
        size_t res;
        asm volatile("cld\n\t"
                     "repne\n\t"
                     "scasb"
                     : "=c" (res), "=&D" (d0)
                     : "1" (s), "a" (0), "0" (0xffffffffu)
                     : "memory");
        return ~res - 1;
}

#if 0
/* This (the state information for strtok()) needs to be defined in
   an object file somewhere? */
extern char *___strtok;

static inline char *
strtok(char *s, const char *ct)
{
    register char *__res;
    asm volatile ("testl %1,%1\n\t"
		  "jne 1f\n\t"
		  "testl %0,%0\n\t"
		  "je 8f\n\t"
		  "movl %0,%1\n"
		  "1:\txorl %0,%0\n\t"
		  "movl $-1,%%ecx\n\t"
		  "xorl %%eax,%%eax\n\t"
		  "cld\n\t"
		  "movl %4,%%edi\n\t"
		  "repne\n\t"
		  "scasb\n\t"
		  "notl %%ecx\n\t"
		  "decl %%ecx\n\t"
		  "je 7f\n\t"			/* empty delimeter-string */
		  "movl %%ecx,%%edx\n"
		  "2:\tlodsb\n\t"
		  "testb %%al,%%al\n\t"
		  "je 7f\n\t"
		  "movl %4,%%edi\n\t"
		  "movl %%edx,%%ecx\n\t"
		  "repne\n\t"
		  "scasb\n\t"
		  "je 2b\n\t"
		  "decl %1\n\t"
		  "cmpb $0,(%1)\n\t"
		  "je 7f\n\t"
		  "movl %1,%0\n"
		  "3:\tlodsb\n\t"
		  "testb %%al,%%al\n\t"
		  "je 5f\n\t"
		  "movl %4,%%edi\n\t"
		  "movl %%edx,%%ecx\n\t"
		  "repne\n\t"
		  "scasb\n\t"
		  "jne 3b\n\t"
		  "decl %1\n\t"
		  "cmpb $0,(%1)\n\t"
		  "je 5f\n\t"
		  "movb $0,(%1)\n\t"
		  "incl %1\n\t"
		  "jmp 6f\n"
		  "5:\txorl %1,%1\n"
		  "6:\tcmpb $0,(%0)\n\t"
		  "jne 7f\n\t"
		  "xorl %0,%0\n"
		  "7:\ttestl %0,%0\n\t"
		  "jne 8f\n\t"
		  "movl %0,%1\n"
		  "8:"
		  : "=b" (__res), "=S" (___strtok)
		  : "0" (___strtok), "1" (s), "g" (ct)
		  : "ax", "cx", "dx", "di", "memory");
    return __res;
}
#endif

static inline void *
memcpy(void *dest, const void *src, size_t n)
{
        int d0, d1, d2, d3;
        asm volatile ("cld\n\t"
                      "movl %%edx, %%ecx\n\t"
                      "shrl $2,%%ecx\n\t"
                      "rep ; movsl\n\t"
                      "testb $1,%%dl\n\t"
                      "je 1f\n\t"
                      "movsb\n"
                      "1:\ttestb $2,%%dl\n\t"
                      "je 2f\n\t"
                      "movsw\n"
                      "2:\n"
                      : "=&S" (d0), "=&D" (d1), "=&d" (d2), "=&a" (d3)
                      : "0" (src), "1" (dest), "2" (n)
                      : "memory", "cx");
    return dest;
}

static inline void *
memmove(void *dest, const void *src, size_t n)
{
        int d0, d1, d2;
        if (dest < src) {
                asm volatile ("cld\n\t"
                              "rep\n\t"
                              "movsb"
                              : "=&D" (d0), "=&S" (d1), "=&c" (d2)
                              : "0" (dest), "1" (src), "2" (n)
                              : "memory");
        }
        else {
                dest = n - 1 + (char *)dest;
                src = n - 1 + (const char *)src;
                asm volatile ("std\n\t"
                              "rep\n\t"
                              "movsb\n\t"
                              "cld"
                              : "=&D" (d0), "=&S" (d1), "=&c" (d2)
                              : "0" (dest), "1" (src), "2" (n)
                              : "memory");
        }
        return dest;
}

static inline int
memcmp(const void *cs, const void *ct, size_t count)
{
        int d0, d1, d2;
        int res;

        asm volatile ("cld\n\t"
                      "repe\n\t"
                      "cmpsb\n\t"
                      "je 1f\n\t"
                      "movl $1,%%eax\n\t"
                      "jb 1f\n\t"
                      "negl %%eax\n"
                      "1:"
                      : "=&D" (d0), "=&S" (d1), "=&c" (d2), "=&a" (res)
                      : "0" (cs), "1" (ct), "2" (count)
                      : "memory");
        return res;
}

static inline void *
memchr(const void *cs, char c, size_t count)
{
    register void * __res;
    if (count == 0)
	return NULL;
    asm ("cld\n\t"
	 "repne\n\t"
	 "scasb\n\t"
	 "je 1f\n\t"
	 "movl $1,%0\n"
	 "1:\tdecl %0"
	 : "=D" (__res)
	 : "a" (c), "D" (cs), "c" (count)
	 : "cx");
    return __res;
}

static inline void *
memset(void *s, char c, size_t count)
{
        int d0, d1, d2;
        asm volatile ("cld\n\t"
                      "rep\n\t"
                      "stosb"
                      : "=&D" (d0), "=&a" (d1), "=&c" (d2)
                      : "0" (s), "1" (c), "2" (count) : "memory");
        return s;
}

static inline void *
memsetw(void *s, u_short w, size_t count)
{
        int d0, d1, d2;
        asm volatile ("cld\n\t"
                      "rep\n\t"
                      "stosw"
                      : "=&D" (d0), "=&a" (d1), "=&c" (d2)
                      : "0" (s), "1" (w), "2" (count) : "memory");
        return s;
}

static inline void *
memsetl(void *s, u_long l, size_t count)
{
        int d0, d1, d2;
        asm volatile ("cld\n\t"
                      "rep\n\t"
                      "stosl"
                      : "=&D" (d0), "=&a" (d1), "=&c" (d2)
                      : "0" (s), "1" (l), "2" (count) : "memory");
    return s;
}

#else /* i386 */

#include <sys/types.h>
#include <string.h>
#include <memory.h>
#define __NO_TYPE_CLASHES
#include <vmm/types.h>

static inline char *
stpcpy(char *dst, const char *src)
{
    while((*dst++ = *src++))
	;
    return dst-1;
}

#endif /* !i386 */

static inline u_char
tolower(register u_char c)
{
    return ((c < 'A') || (c > 'Z')) ? c : (c + ('a' - 'A'));
}

/* This is borrowed from the Linux C library :) */
static inline int
strcasecmp(const char *s1, const char *s2)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  register int ret;
  unsigned char c1;

  if (p1 == p2)
    return 0;

  for (; !(ret = (c1 = tolower(*p1)) - tolower(*p2)); p1++, p2++)
    if (c1 == '\0') break;
  return ret;
}

#endif /* _VMM_STRING_H */
