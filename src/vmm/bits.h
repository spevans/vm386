/* bits.h -- Bit twidding operations.
   John Harper. */

#ifndef _VMM_BITS_H
#define _VMM_BITS_H

#include <vmm/types.h>

static inline void
set_bit(void *bit_string, int bit_offset)
{
    ((char *)bit_string)[bit_offset / 8] |= (1 << (bit_offset % 8));
}

static inline void
clear_bit(void *bit_string, int bit_offset)
{
    ((char *)bit_string)[bit_offset / 8] &= ~(1 << (bit_offset % 8));
}

static inline int
test_bit(void *bit_string, int bit_offset)
{
    return ((char *)bit_string)[bit_offset / 8] & (1 << (bit_offset % 8));
}

#define BSF(word, result) asm ("bsfl %1,%0" : "=r" (result) : "r" (word))

/* Scan the bit-string BITS for the first zero bit and return its position.
   LEN is the number of bits to scan, note that it actually searches to the
   end of the word that the last bit lives in. -1 is returned if no zero
   bit is found. */
static inline int
find_zero_bit(void *bits, int len)
{
    long result;
    asm ("movl $-1,%%eax\n\t"
	 "cld\n\t"
	 "repe; scasl\n\t"		/* Find first word which != -1 */
	 "je 1f\n\t"			/* No match? Return -1 if so. */
	 "subl $4,%%edi\n\t"		/* Back to the non-matching word. */
	 "movl (%%edi),%%edx\n\t"
	 "notl %%edx\n\t"
	 "bsfl %%edx,%%eax\n\t"		/* Find the first zero bit. */
	 "subl %%ebx,%%edi\n\t"		/* The word's offset... */
	 "shll $3,%%edi\n\t"		/* in bits. */
	 "addl %%edi,%%eax\n"		/* Bit position of the zero. */
	 "1:"
	 : "=a" (result)
	 : "D" (bits), "b" (bits), "c" ((len + 31) / 32)
	 : "dx", "di");
    return result;
}
 
#endif /* _VMM_BITS_H */
