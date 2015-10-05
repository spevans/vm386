/* crt0.S -- the first object file in each module. */

#define CONCAT3(a,b,c) __CONCAT3(a,b,c)
#define __CONCAT3(a,b,c) a ## b ## c

#define LABEL(x) __LABEL(x)
#define __LABEL(x) x ## :

#define MOD_STRUCT CONCAT3(, MODNAME, _module)
#define MOD_START  CONCAT3(_, MODNAME, _start)
#define MOD_END    CONCAT3(_, MODNAME, _end)

	.text
	.globl	MOD_START
LABEL(MOD_START)
	.long	MOD_STRUCT
	.long	kernel
	.long	MOD_END
