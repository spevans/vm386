/* disk.c -- VBIOS INT 13 handling.
   John Harper. */

#include <vmm/vbios.h>
#include <vmm/vm.h>
#include <vmm/vide.h>
#include <vmm/vfloppy.h>
#include <vmm/kernel.h>
#include <vmm/hdreg.h>
#include <vmm/tasks.h>
#include <vmm/fs.h>
#include <vmm/segment.h>

#define kprintf kernel->printf

extern struct vide_module *vide;
extern struct vfloppy_module *vfloppy;
extern struct fs_module *fs;

static void
get_hd_stat(struct vm *vm, struct vm86_regs *regs)
{
    u_char bstat, stat, err;
    if(vide->get_status(vm, &stat, &err))
    {
	if(stat & HD_STAT_ERR)
	{
	    if(err & HD_ERR_AMNF)
		bstat = 0x02;
	    else if(err & HD_ERR_IDNF)
		bstat = 0x04;
	    else if(err & HD_ERR_UNK)
		bstat = 0x11;
	    else
		bstat = 0x20;
	}
	else
	    bstat = 0;
    }
    else
	bstat = 0xBB;
    regs->eax = SET8H(regs->eax, bstat);
    if(bstat == 0)
	CLC(regs);
    else
	STC(regs);
}

static void
get_fd_stat(struct vm *vm, struct vm86_regs *regs)
{
    u_char bstat, stat, err;
    if(vfloppy->get_status(vm, &stat, &err))
    {
	if(stat & HD_STAT_ERR)
	{
	    if(err & HD_ERR_AMNF)
		bstat = 0x02;
	    else if(err & HD_ERR_IDNF)
		bstat = 0x04;
	    else if(err & HD_ERR_UNK)
		bstat = 0x11;
	    else
		bstat = 0x20;
	}
	else
	    bstat = 0;
    }
    else
	bstat = 0xBB;
    regs->eax = SET8H(regs->eax, bstat);
    if(bstat == 0)
	CLC(regs);
    else
	STC(regs);
}

void
vbios_disk_handler(struct vm *vm, struct vm86_regs *regs, struct vbios *vbios)
{
    u_char func = GET8H(regs->eax);
    DB(("vbios_disk_handler: func=%x eax=%x edx=%x ecx=%x eflags=%x\n",
	func, regs->eax, regs->edx, regs->ecx, regs->eflags));
    if(func == 0)
    {
	/* Reset. */
	regs->eax = SET8H(regs->eax, 0x00);
	CLC(regs);
    }
    else if((regs->edx & 0x80) == 0)
    {
	/* Diskette functions. */
	if(((regs->edx & 0x7f) != 0) && (func != 8))
	{
	    regs->eax = SET8H(regs->eax, 0xff);
	    STC(regs);
	}
	else
	{	    
	    switch(func)
	    {
	    case 1:
		get_fd_stat(vm, regs);
		break;
	    case 2:
		{
		    int head, cyl, sect, count;
		    u_long buf;
		    head = GET8H(regs->edx);
		    cyl = GET8H(regs->ecx) | ((GET8(regs->ecx) & 0xc0) << 2);
		    sect = GET8(regs->ecx) & 0x3f;
		    count = GET8(regs->eax);
		    buf = (regs->es << 4) + GET16(regs->ebx);
		    count = vfloppy->read_user_blocks(vm, 0, 
						 head, cyl, sect,
						 count, (void *)buf);
		    get_fd_stat(vm, regs);
                    break;
		}
	    case 3:
	    case 4:
		regs->eax = SET16(regs->eax, 0x0300);
		STC(regs);
		break;
	    case 5:
		regs->eax = SET8H(regs->eax, 0x03);
		STC(regs);
		break;
	    case 8:
		if((regs->edx & 0x7f) == 0)
		{
		    regs->eax = SET16(regs->eax, 0x0000);
		    regs->ebx = SET16(regs->ebx, 0x0004);
		    regs->ecx = SET16(regs->ecx, 0x5012);
		    regs->edx = SET16(regs->edx, 0x0201);
		    regs->edi = SET16(regs->edi,
				      get_user_short((u_short *)(0x1e*4)));
		    regs->es = get_user_short((u_short *)(0x1e*4)+2);
		}
		else
		{
		    regs->eax = SET16(regs->eax, 0x0000);
		    regs->ebx = SET16(regs->ebx, 0x0000);
		    regs->ecx = SET16(regs->ecx, 0x0000);
		    regs->edx = SET16(regs->edx, 0x0000);
		}
		CLC(regs);
		break;
	    default:
		regs->eax = SET8H(regs->eax, 0x01);
		STC(regs);
	    }
	}
	put_user_byte(GET8H(regs->eax), &BIOS_DATA->drive_status);
    }
    else
    {
	/* Hard disk functions. */
	if(((regs->edx & 0x7f) != 0)
	    && (func != 21))
	{
	    regs->eax = SET8H(regs->eax, 0xff);
	    STC(regs);
	}
	else
	{
	    switch(func)
	    {
	    case 1: case 20:
		get_hd_stat(vm, regs);
		break;
	    case 2:
	    case 3:
		{
		    int head, cyl, sect, count;
		    u_long buf;
		    head = GET8H(regs->edx);
		    cyl = GET8H(regs->ecx) | ((GET8(regs->ecx) & 0xc0) << 2);
		    sect = GET8(regs->ecx) & 0x3f;
		    count = GET8(regs->eax);
		    buf = (regs->es << 4) + GET16(regs->ebx);
		    if(func == 2)
		    {
			vide->read_user_blocks(vm, 0, head, cyl, sect,
					       count, (void *)buf);
		    }
		    else
		    {
			vide->write_user_blocks(vm, 0, head, cyl, sect,
						count, (void *)buf);
		    }
		    get_hd_stat(vm, regs);
		    break;
		}
	    case 5:
		{
		    u_int heads, cyls, sects;
		    u_short *buf = (u_short *)((regs->es << 4)
					       + GET16(regs->ebx));
		    if(vide->get_geom(vm, &heads, &cyls, &sects))
		    {
			int i;
			for(i = 0; i < sects; i++)
			    put_user_short(i, &buf[i]);
		    }
		}
		/* FALL THROUGH */
	    case 0: case 4:
	    case 6: case 7:
	    case 12: case 13:
	    case 16: case 17:
	    case 25:
		regs->eax = SET8H(regs->eax, 0);
		CLC(regs);
		break;
	    case 8:
		{
		    u_int heads, cyls, sects;
		    if(vide->get_geom(vm, &heads, &cyls, &sects))
		    {
			heads--; cyls--;
			regs->edx = SET16(regs->edx, 1 | (heads << 8));
			regs->ecx = SET16(regs->ecx,
					  ((cyls & 0xff) << 8)
					  | (sects & 0x3f)
					  | ((cyls & 0x0300) >> 2));
		    }
		    else
			regs->edx = SET16(regs->edx, 0);
		    break;
		}
	    case 9:
		/* ?? */
		break;
	    case 21:
		if(regs->edx & 0x1)
		    regs->eax = SET8H(regs->eax, 0);
		else
		{
		    u_int heads, cyls, sects;
		    regs->eax = SET8H(regs->eax, 3);
		    vide->get_geom(vm, &heads, &cyls, &sects);
		    regs->ecx = SET16(regs->ecx, (heads * cyls * sects) >> 16);
		    regs->edx = SET16(regs->edx, (heads * cyls * sects));
		}
		break;
	    default:
		unimplemented("vbios: int 13", regs);
		regs->eax = SET8H(regs->eax, 1);
		STC(regs);
	    }
	    put_user_byte(GET8H(regs->eax), &BIOS_DATA->fixed_disk_status);
	}
    }
    DB(("vbios_disk_handler: finished, eax=%x cs:eip=%x:%x eflags=%x\n",
	regs->eax, regs->cs, regs->eip, regs->eflags));
}
