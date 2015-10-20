/* debug.c -- Simple use of the debugging registers.
   John Harper. */

#include <vmm/traps.h>
#include <vmm/kernel.h>
#include <vmm/page.h>
#include <vmm/debug.h>

void
set_debug_reg(int reg_set, u_long addr, int type, int size)
{
    u_long dr7_mask;
    switch(reg_set)
    {
    case 0:
	set_dr0(addr);
	dr7_mask = 0xFFF000FC;
	break;
    case 1:
	set_dr1(addr);
	dr7_mask = 0xFF0F00F3;
	break;
    case 2:
	set_dr2(addr);
	dr7_mask = 0xF0FF00CF;
	break;
    case 3:
	set_dr3(addr);
	dr7_mask = 0x0FFF003F;
	break;
    default:
	return;
    }
    set_dr7((get_dr7() & dr7_mask)
	    | (((size << 2) | type) << (16+reg_set*2))	/* LEN and R/W */
	    | (2 << (reg_set*2)) 			/* Gx */
	    | ((type & 1) ? 0x20 : 0));			/* GE if data */
    kprintf("Set debug reg %d to linear addr %#lx (type=%2d, size=%d)\n",
	    reg_set, addr, type, size);
}
