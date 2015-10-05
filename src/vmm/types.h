/* types.h -- system wide types and definitions for VMM/386
   John Harper. */

#ifndef _VMM_TYPES_H
#define _VMM_TYPES_H

#include <stdint.h>

/* This file assumes 32-bit ints and longs. */

#ifndef __NO_TYPE_CLASHES
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
#endif

typedef uint32_t time32_t;

typedef int int32;
typedef unsigned int u_int32;
typedef short int16;
typedef unsigned short u_int16;
typedef char int8;
typedef unsigned char u_int8;

#ifndef __NO_TYPE_CLASHES
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef int ptrdiff_t;
#endif

typedef char *caddr_t;

#endif /* __NO_TYPE_CLASHES */

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Maybe this should be an int? */
typedef char bool;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Round up VAL to the next multiple of MOD. */
extern inline u_long
round_to(u_long val, u_long mod)
{
    return (val + (mod - 1)) & ~(mod - 1);
}

extern inline int
max(int v1, int v2)
{
    return v1 > v2 ? v1 : v2;
}

extern inline int
min(int v1, int v2)
{
    return v1 < v2 ? v1 : v2;
}

#ifdef DEBUG
# define DB(x) kprintf x
#else
# define DB(x) do ; while(0)
#endif

#endif /* _VMM_TYPES_H */
