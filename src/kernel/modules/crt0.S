/* crt0.S -- the first object file in each module. */

#define CONCAT3(a,b,c) __CONCAT3(a,b,c)
#define __CONCAT3(a,b,c) a ## b ## c

#define LABEL(x) __LABEL(x)
#define __LABEL(x) x ## :

#define MOD_STRUCT CONCAT3(_, MODNAME, _module)
#define MOD_START  CONCAT3(__, MODNAME, _start)
#define MOD_END    CONCAT3(__, MODNAME, _end)

	.text
	.globl	MOD_START
LABEL(MOD_START)
	.long	MOD_STRUCT
	.long	_kernel
	.long	MOD_END
