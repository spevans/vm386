/* misc.c -- Miscellaneous VBIOS stuff.
   John Harper. */

#include <vmm/vbios.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/mc146818rtc.h>
#include <vmm/vprinter.h>
#include <vmm/vcmos.h>
#include <vmm/segment.h>

#define kprintf kernel->printf

extern struct vprinter_module *vprinter;

extern struct vm_module *vm;
extern struct vcmos_module *vcmos;

void
unimplemented(char *name, struct vm86_regs *regs)
{
  kprintf("Unimplemented function module: %s\n", name);
  kprintf("AX: %04X  BX: %04X  CX: %04X  DX: %04X\n",
    GET16(regs->eax), GET16(regs->ebx), GET16(regs->ecx), GET16(regs->edx));
}


// FIXME
void
vbios_ser_handler(__attribute__ ((unused)) struct vm *vm, struct vm86_regs *regs)
{
    u_char func = GET8H(regs->eax);
    switch(func)
    {
    case 0:
    case 3:
	regs->eax = SET16(regs->eax, 0x8000);
	break;
    case 1:
    case 2:
	regs->eax = SET8H(regs->eax, 0x80);
	break;
    default:
        unimplemented("vbios int13", regs);
    }
}

void
vbios_par_handler(struct vm *vm, struct vm86_regs *regs)
{
    u_char func = GET8H(regs->eax);
    if(vprinter != NULL) {
        switch(func) {
            case 0:	/* write character to printer */
                vprinter->printer_write_char(vm, regs);
            break;

            case 1:
                vprinter->printer_initialise(vm, regs);
            break;

            case 2:
                vprinter->printer_get_status(vm, regs);
            break; 

            default:
                unimplemented("vbios int17", regs);
        }
    }
    else {
        switch(func)
        {
        case 0:
        case 1:
        case 2:
     	    regs->eax = SET8H(regs->eax, 0x01);
	    break;
        default:
            unimplemented("vbios int17", regs);
        }
   }
}

/* int 1A */

void
vbios_timer_handler(struct vm *vm, struct vm86_regs *regs)
{
    u_char h,s,y,m,d; 
    u_short dayc;
    u_char func = GET8H(regs->eax);

    switch(func)
    {
    case 0:			/* Read system-timer counters. */
	{
	    u_long ticks = get_user_long(&BIOS_DATA->timer_counter);
	    regs->edx = SET16(regs->ecx, ticks);
	    regs->ecx = SET16(regs->edx, ticks >> 16);
	    regs->eax = SET8(regs->eax,
			     get_user_byte(&BIOS_DATA->timer_overflow));
	}
	break;

    case 1:			/* Set system-timer counters. */
	put_user_long(GET16(regs->edx) | (GET16(regs->ecx) << 16),
		      &BIOS_DATA->timer_counter);
	break;

    case 2:			/* Get CMOS time. */
        if(vcmos != NULL) {
            vcmos->get_bcd_time(vm, regs);
        } else {
            h = CMOS_READ(RTC_HOURS);
            m = CMOS_READ(RTC_MINUTES);
            s = CMOS_READ(RTC_SECONDS);
            if(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) {
                BIN_TO_BCD(h);
                BIN_TO_BCD(m);
                BIN_TO_BCD(s);
            }
            regs->ecx = SET8H(regs->ecx, h);
            regs->ecx = SET8(regs->ecx, m);
            regs->edx = SET8H(regs->edx, s);
            regs->edx = SET8(regs->edx,
                (CMOS_READ(RTC_CONTROL) & RTC_DST_EN) ? 0 : 1);
            CLC(regs);
        }
        break;

    case 3:		/* Set CMOS time. DISALLOWED if no virtual cmos */
        if(vcmos != NULL) {
            vcmos->set_bcd_time(vm, regs);
        }
        break;

    case 4:			/* Get CMOS date. */
        if(vcmos != NULL) {
            vcmos->get_bcd_date(vm, regs);
        } else {
            y = CMOS_READ(RTC_YEAR);
            m = CMOS_READ(RTC_MONTH);
            d = CMOS_READ(RTC_DAY_OF_MONTH);
            if(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) {
                BIN_TO_BCD(y);
                BIN_TO_BCD(m);
                BIN_TO_BCD(d);
            }
            regs->ecx = SET8H(regs->ecx, CMOS_READ(0x32));
            regs->ecx = SET8(regs->ecx, y);
            regs->edx = SET8H(regs->edx, m);
            regs->edx = SET8(regs->edx, d);
            CLC(regs);
        }
        break;
    
    case 5:		/* Set CMOS date. DISALLOWED if no virtual cmos */
        if(vcmos != NULL) {
            vcmos->set_bcd_date(vm, regs);
        }
        break;

    case 6:		/* Set CMOS alarm. */
        if(vcmos != NULL) {
            vcmos->set_alarm(vm, regs);
        }
        break;

    case 7:		/* Reset CMOS alarm.*/
        if(vcmos != NULL) {
            vcmos->reset_alarm(vm, regs);
        }
        break;

    case 0xA:		/* Read System Day Counter */
	dayc = get_user_short((u_short *)0x467);
	regs->ecx = SET16(regs->ecx, dayc);
        CLC(regs);
        break;

    case 0xB:		/* Set System Day Counter */
        put_user_short(GET16(regs->ecx), (u_short *)0x467);
        break;

    default:
        unimplemented("vbios int1A", regs);
    }
}
	
void
vbios_sys_handler(struct vm *vmach, struct vm86_regs *regs, struct vbios *vbios)
{
    u_char func = GET8H(regs->eax);
    switch(func)
    {
    case 134:			/* Wait. */
	set_timer_sem(&vbios->int15_timer,
		       (GET16(regs->ecx) + (GET16(regs->edx) << 16)));
	kernel->add_timer(&vbios->int15_timer);
	wait(&vbios->int15_timer.action.sem);
	CLC(regs);
	break;

    case 135:			/* Move block in protected mode. */
	{
	    bool old_a20_state = vmach->a20_state;
	    char *src, *dst;
	    desc_table *gdt;
	    desc_table tmp;
	    size_t len;
	    len = GET16(regs->ecx) * 2;
	    gdt = (desc_table *)((regs->es << 4) + GET16(regs->esi));
	    memcpy_from_user(&tmp, &gdt[2], sizeof(tmp));
	    src = (char *)((tmp.lo >> 16) | ((tmp.hi & 0xff) << 16) | (tmp.hi & 0xff000000));
	    memcpy_from_user(&tmp, &gdt[3], sizeof(tmp));
	    dst = (char *)((tmp.lo >> 16) | ((tmp.hi & 0xff) << 16) | (tmp.hi & 0xff000000));
	    if(!old_a20_state)
		vm->set_gate_a20(vmach, TRUE);
	    memcpy_user(dst, src, len);
	    vm->set_gate_a20(vmach, old_a20_state);
	    regs->eax = SET8H(regs->eax, 0);
	    regs->eflags = (regs->eflags & ~FLAGS_CF) | 0x40; /* ~CF, ZF */
	    break;
	}

    case 136:			/* Ext. memory determination. */
	regs->eax = SET16(regs->eax, vmach->hardware.extended_mem);
	CLC(regs);
	break;

    case 192:			/* Get system config. */
	regs->es = 0xfffe;
	regs->ebx = SET16(regs->ebx, 0);
	regs->eax = SET8H(regs->eax, 0);
	CLC(regs);
	break;

    case 128:			/* Device open. */
    case 129:			/* Device close. */
    case 130:			/* Program determination. */
    case 131:			/* Event wait. */
    case 132:			/* Joystick read. */
    case 133:			/* SysReq pressed. */
    case 137:			/* Switch to protected mode. */
    case 144:			/* Device busy. */
    case 145:			/* Interrupt complete. */
    default:
	STC(regs);
        unimplemented("vbios int15", regs);
    }
}
