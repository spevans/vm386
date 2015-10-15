/* fault.c -- Virtual machine fault handling.
   John Harper. */

#include <vmm/vm.h>
#include <vmm/tasks.h>
#include <vmm/tty.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/segment.h>
#include <vmm/irq.h>
#include <vmm/page.h>
#include <vmm/traps.h>
#include <vmm/vpic.h>

#define kprintf kernel->printf

#define PFX_ADDR 1
#define PFX_LOCK 2
#define PFX_OP   4
#define PFX_CS   8
#define PFX_DS   16
#define PFX_ES   32
#define PFX_FS   64
#define PFX_GS   128
#define PFX_SS   256

static inline u_char
pop_cs(struct vm86_regs *regs)
{
    u_char c = get_user_byte((u_char *)((regs->cs << 4) + GET16(regs->eip)));
    regs->eip = SET16(regs->eip, regs->eip + 1);
    return c;
}

static inline u_char
read_cs(struct vm86_regs *regs)
{
    return get_user_byte((u_char *)((regs->cs << 4) + GET16(regs->eip)));
}

static inline u_short
get_data_seg(struct vm86_regs *regs, u_long prefixes)
{
    if((prefixes & (PFX_CS | PFX_ES | PFX_FS | PFX_GS | PFX_SS)) == 0)
	return regs->ds;
    else if(prefixes & PFX_CS)
	return regs->cs;
    else if(prefixes & PFX_ES)
	return regs->es;
    else if(prefixes & PFX_FS)
	return regs->fs;
    else if(prefixes & PFX_GS)
	return regs->gs;
    else if(prefixes & PFX_SS)
	return regs->ss;
    else
	return regs->ds;
}

static void
push_di(struct vm86_regs *regs, u_long prefixes, int size, u_long val)
{
    u_long di = (prefixes & PFX_ADDR) ? regs->edi : regs->edi & 0xffff;
    switch(size)
    {
    case 1:
	put_user_byte((u_char)val, (u_char *)((regs->es << 4) + di));
	break;
    case 2:
	put_user_short((u_short)val, (u_short *)((regs->es << 4) + di));
	break;
    case 4:
	put_user_long(val, (u_long *)((regs->es << 4) + di));
    }
    if(prefixes & PFX_ADDR)
    {
	if(regs->eflags & FLAGS_DF)
	    regs->edi -= size;
	else
	    regs->edi += size;
    }
    else
    {
	if(regs->eflags & FLAGS_DF)
	    regs->edi -= size;
	else
	    regs->edi += size;
    }
}

static u_long
pop_si(struct vm86_regs *regs, u_long prefixes, int size)
{
    u_long res;
    u_long si = (prefixes & PFX_ADDR) ? regs->esi : regs->esi & 0xffff;
    u_short seg = get_data_seg(regs, prefixes);
    switch(size)
    {
    case 1:
	res = (u_long)get_user_byte((u_char *)((seg << 4) + si));
	break;
    case 2:
	res = (u_long)get_user_short((u_short *)((seg << 4) + si));
	break;
    case 4:
	res = get_user_long((u_long *)((seg << 4) + si));
	break;
    default:
	res = 0;
    }
    if(prefixes & PFX_ADDR)
    {
	if(regs->eflags & FLAGS_DF)
	    regs->esi -= size;
	else
	    regs->esi += size;
    }
    else
    {
	if(regs->eflags & FLAGS_DF)
            regs->esi -= size;
	else
	    regs->esi += size;
    }
    return res;
}

static u_long
pop_sp(struct vm86_regs *regs, int size)
{
    u_long res;
    switch(size)
    {
    case 2:
	res = get_user_short((u_short *)((regs->ss << 4) + GET16(regs->esp)));
	regs->esp = SET16(regs->esp, GET16(regs->esp) + 2);
	break;
    case 4:
	res = get_user_long((u_long *)((regs->ss << 4) + GET16(regs->esp)));
	regs->esp = SET16(regs->esp, GET16(regs->esp) + 4);
	break;
    default:
	res = 0;
    }
    return res;
}

static void
push_sp(struct vm86_regs *regs, int size, u_long value)
{
    switch(size)
    {
    case 2:
	regs->esp = SET16(regs->esp, GET16(regs->esp) - 2);
	put_user_short(value, (u_short *)((regs->ss << 4) + GET16(regs->esp)));
	break;
    case 4:
	regs->esp = SET16(regs->esp, GET16(regs->esp) - 4);
	put_user_long(value, (u_long *)((regs->ss << 4) + GET16(regs->esp)));
	break;
    }
}

static void
push_far_sp(struct vm *vm, struct vm86_regs *regs, u_long value)
{
    regs->esp = SET16(regs->esp, GET16(regs->esp) - 2);
    kernel->put_pd_val(vm->task->page_dir, 2, value,
		       (regs->ss << 4) + GET16(regs->esp));
}

#define REGS ((struct vm86_regs *)regs)

/* Set up the current stack frame of VM (i.e. vm->task->frame) to simulate
   an `INT VECTOR' instruction. */
void
simulate_vm_int(struct vm *vm, struct trap_regs *regs, int vector)
{
    u_long vec_phys_addr;
    DB(("simulate_vm_int: vm=%p vector=%d\n", vm, vector));
    vec_phys_addr = kernel->lin_to_phys(vm->task->page_dir,
					vector * 4);
    push_far_sp(vm, REGS, ((vm->virtual_eflags & ~USER_EFLAGS)
			   | (regs->eflags & USER_EFLAGS)));
    vm->virtual_eflags &= ~(FLAGS_IF | FLAGS_TF);
    push_far_sp(vm, REGS, REGS->cs);
    push_far_sp(vm, REGS, REGS->eip);
    REGS->cs = *TO_LOGICAL(vec_phys_addr+2, u_short *);
    REGS->eip = SET16(REGS->eip, *TO_LOGICAL(vec_phys_addr, u_short *));
    DB(("simulate_vm_int: cs:eip=%x:%x eflags=%x\n", REGS->cs, REGS->eip,
	REGS->eflags));
}

static inline u_long
get_prefixes(struct vm86_regs *regs)
{
    u_long prefixes = 0;
    while(1)
    {
	switch(pop_cs(regs))
	{
	case 0x67:
	    prefixes |= PFX_ADDR;
	    break;
	case 0x80:
	    prefixes |= PFX_LOCK;
	    break;
	case 0x66:
	    prefixes |= PFX_OP;
	    break;
	case 0x2e:
	    prefixes |= PFX_CS;
	    break;
	case 0x3e:
	    prefixes |= PFX_DS;
	    break;
	case 0x26:
	    prefixes |= PFX_ES;
	    break;
	case 0x64:
	    prefixes |= PFX_FS;
	    break;
	case 0x65:
	    prefixes |= PFX_GS;
	    break;
	case 0x36:
	    prefixes |= PFX_SS;
	    break;
	default:
	    regs->eip = SET16(regs->eip, GET16(regs->eip) - 1);
	    return prefixes;
	}
    }
}

void
vm_gpe_handler(struct trap_regs *regs)
{
    static struct vpic_module *vpic = NULL;

    struct vm *vm = GET_TASK_VM(kernel->current_task);
    u_long prefixes;
    u_long orig_eip = regs->eip;
    u_char byte;

    if (!vpic) {
        // FIXME, should vpic be used or should eflags jsut be checked for IF
        vpic =(struct vpic_module *)kernel->open_module("vpic", SYS_VER);
    }

    DB(("vm_gpe: vm=%p cs:eip=%x:%x ec=%x\n", vm, REGS->cs, REGS->eip,
	REGS->error_code));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("General Protection violation!\n");
	kprintf("Error: %08X\n", regs->error_code);
	kernel->dump_regs(regs, TRUE);
    }

    prefixes = get_prefixes(REGS);
    switch((byte = pop_cs(REGS)))
    {
    case 0xe4:			/* IN AL,Ib */
    case 0xe5:			/* IN eAX,Ib */
    case 0xec:			/* IN AL,DX */
    case 0xed:			/* IN eAX,DX */
    case 0x6c:			/* INSB Yb,DX */
    case 0x6d:			/* INSW/D Yv,DX */
	{
	    u_short port, val;
	    int size;
	    struct io_handler *ioh;
	    u_long mask;
	    DB(("vm_gpe: IN (opcode=%02x)\n", byte));
	    if(byte & 0x08)
		port = GET16(REGS->edx);
	    else
		port = pop_cs(REGS);
	    if(byte & 1)
	    {
		if(prefixes & PFX_OP)
		    size = 4, mask = 0xffffffff;
		else
		    size = 2, mask = 0x0000ffff;
	    }
	    else
		size = 1, mask = 0x000000ff;
	    ioh = get_io_handler(vm, port);
	    if(ioh == NULL && verbose_io && port != 0x84)
		kprintf("vm_gpe: unhandled IN %#x\n", port);
	    val = (ioh ? ioh->in(vm, port, size) : (u_long)-1) & mask;
	    if(byte & 0x80)
		REGS->eax = (REGS->eax & ~mask) | val;
	    else
		push_di(REGS, prefixes, size, val);
	    break;
	}

    case 0xe6:			/* OUT Ib,AL */
    case 0xe7:			/* OUT Ib,eAX */
    case 0xee:			/* OUT DX,AL */
    case 0xef:			/* OUT DX,eAX */
    case 0x6e:			/* OUTSB DX,Xb */
    case 0x6f:			/* OUTSW/D DX,Xv */
	{
	    u_short port;
	    int size;
	    struct io_handler *ioh;
	    u_long mask;
	    DB(("vm_gpe: OUT (opcode=%02x)\n", byte));
	    if(byte & 0x08)
		port = GET16(REGS->edx);
	    else
		port = pop_cs(REGS);
	    ioh = get_io_handler(vm, port);
	    if(ioh == NULL)
	    {
		if(verbose_io)
		    kprintf("vm_gpe: unhandled OUT %#x\n", port);
		break;
	    }
	    if(byte & 1)
	    {
		if(prefixes & PFX_OP)
		    size = 4, mask = 0xffffffff;
		else
		    size = 2, mask = 0x0000ffff;
	    }
	    else
		size = 1, mask = 0x000000ff;
	    if(byte & 0x80)
		ioh->out(vm, port, size, REGS->eax & mask);
	    else
		ioh->out(vm, port, size, pop_si(REGS, prefixes, size));
	    break;
	}

    case 0xfa:			/* CLI */
	DB(("vm_gpe: CLI\n"));
	vm->virtual_eflags &= ~FLAGS_IF;
	if (vpic) {
            vpic->IF_disabled(vm);
        }
	break;

    case 0xfb:			/* STI */
	DB(("vm_gpe: STI\n"));
	vm->virtual_eflags |= FLAGS_IF;
	if(read_cs(REGS) == 0xf4)
	{
	    /* A `sti; hlt' combination. Since interrupts shouldn't
	       be unmasked until after the next instruction, do the
	       hlt now (in case any interrupts are pending and would
	       get dispatched before the hlt).
	         Really I should use the TF bit to do this properly,
	       but I can't make it work :(  What other troublesome
	       combinations might there be? */
	    DB(("vm_gpe: HLT\n"));
	    pop_cs(REGS);
	    vm->hlted = TRUE;
	    kernel->suspend_task(vm->task);
	}
        if (vpic) {
            vpic->IF_enabled(vm);
        }
	break;

    case 0x9c:			/* PUSHF Fv */
	if(prefixes & PFX_OP)
	{
	    DB(("vm_gpe: PUSHFD\n"));
	    push_sp(REGS, 4, ((vm->virtual_eflags & ~USER_EFLAGS)
			      | (REGS->eflags & USER_EFLAGS)));
	}
	else
	{
	    DB(("vm_gpe: PUSHF\n"));
	    push_sp(REGS, 2, ((vm->virtual_eflags & ~USER_EFLAGS)
			      | (REGS->eflags & USER_EFLAGS)));
	}
	break;

    case 0x9d:			/* POPF Fv */
	if(prefixes & PFX_OP)
	{
	    DB(("vm_gpe: POPFD\n"));
	    vm->virtual_eflags = pop_sp(REGS, 4);
	}
	else
	{
	    DB(("vm_gpe: POPF\n"));
	    vm->virtual_eflags = SET16(vm->virtual_eflags, pop_sp(REGS, 2));
	}
	REGS->eflags = ((vm->virtual_eflags & USER_EFLAGS)
			| FLAGS_IF | EFLAGS_VM);
        if (vpic) {
            if(vm->virtual_eflags & FLAGS_IF)
                vpic->IF_enabled(vm);
            else
                vpic->IF_disabled(vm);
        }
	break;

    case 0xcc:			/* INT 3 */
	/* According to the 386 manual INT 3 in V86 mode should cause
	   a normal INT 3 trap, but unless I'm doing something wrong
	   it seems to make a GPE instead!? */
	DB(("vm_gpe: INT 3\n"));
	simulate_vm_int(vm, regs, 3);
	break;

    case 0xcd:			/* INT Ib */
	{
	    u_char vec = pop_cs(REGS);
	    DB(("vm_gpe: INT 0x%x\n", vec));
	    simulate_vm_int(vm, regs, vec);
	}
	break;

    case 0xcf:			/* IRET */
	DB(("vm_gpe: IRET\n"));
	if(prefixes & PFX_OP)
	    REGS->eip = pop_sp(REGS, 4);
	else
	    REGS->eip = SET16(REGS->eip, pop_sp(REGS, 2));
	REGS->cs = pop_sp(REGS, 2);
	if(prefixes & PFX_OP)
	    vm->virtual_eflags = pop_sp(REGS, 4);
	else
	    vm->virtual_eflags = SET16(vm->virtual_eflags, pop_sp(REGS, 2));
	REGS->eflags = ((vm->virtual_eflags & USER_EFLAGS)
			| FLAGS_IF | EFLAGS_VM);
        if (vpic) {
            if(vm->virtual_eflags & FLAGS_IF)
                vpic->IF_enabled(vm);
            else
                vpic->IF_disabled(vm);
        }
	break;

    case 0xf4:			/* HLT */
	DB(("vm_gpe: HLT\n"));
	vm->hlted = TRUE;
	kernel->suspend_task(vm->task);
	break;

    case 0xf3:			/* REP/REPE */
	switch((byte = pop_cs(REGS)))
	{
	case 0x6c:		/* REP INSB Yb,DX */
	case 0x6d:		/* REP INSW/D Yv,DX */
	    {
		u_long addr = REGS->edi;
		u_long base = REGS->es << 4;
		u_long count = REGS->ecx;
		u_short port = GET16(regs->edx);
		u_long addr_mask;
		u_int size;
		struct io_handler *ioh = get_io_handler(vm, port);
		DB(("vm_gpe: REP INS (addr=%x base=%x count=%#x port=%#x)\n",
		    addr, base, count, port));
		if(ioh == NULL && verbose_io)
		    kprintf("vm_gpe: unhandled INS %#x\n", port);
		addr_mask = ((byte & 1)
			     ? ((prefixes & PFX_ADDR)
				? 0xffffffff : 0x0000ffff)
			     : 0x000000ff);
		size = (byte & 1) ? ((prefixes & PFX_OP) ? 4 : 2) : 1;
		addr &= addr_mask;
		count &= (prefixes & PFX_ADDR) ? 0xffffffff : 0x0000ffff;
		while(count-- != 0)
		{
		    u_long val = ioh ? ioh->in(vm, port, size) : 0xffffffff;
		    switch(size)
		    {
		    case 1:
			put_user_byte(val, (u_char *)(base + addr));
			break;
		    case 2:
			put_user_short(val, (u_short *)(base + addr));
			break;
		    case 4:
			put_user_long(val, (u_long *)(base + addr));
			break;
		    }
		    addr = (addr + size) & addr_mask;
		}
		REGS->ecx = REGS->ecx & ~addr_mask;
		break;
	    }

	case 0x6e:		/* OUTSB DX,Xb */
	case 0x6f:		/* OUTSW/D DX,Xv */
	    {
		u_long addr = REGS->esi;
		u_long base = get_data_seg(REGS, prefixes) << 4;
		u_long count = REGS->ecx;
		u_short port = GET16(regs->edx);
		u_long addr_mask;
		u_int size;
		struct io_handler *ioh = get_io_handler(vm, port);
		DB(("vm_gpe: REP OUTS (addr=%x base=%x count=%#x port=%#x)\n",
		    addr, base, count, port));
		if(ioh == NULL && verbose_io)
		    kprintf("vm_gpe: unhandled OUTS %#x\n", port);
		addr_mask = ((byte & 1)
			     ? ((prefixes & PFX_ADDR)
				? 0xffffffff : 0x0000ffff)
			     : 0x000000ff);
		if(ioh != NULL)
		{
		    size = (byte & 1) ? ((prefixes & PFX_OP) ? 4 : 2) : 1;
		    addr &= addr_mask;
		    count &= (prefixes & PFX_ADDR) ? 0xffffffff : 0x0000ffff;
		    while(count-- != 0)
		    {
			u_long val;
			switch(size)
			{
			case 1:
			    val = get_user_byte((u_char *)(base + addr));
			    break;
			case 2:
			    val = get_user_short((u_short *)(base + addr));
			    break;
			case 4:
			default:
			    val = get_user_long((u_long *)(base + addr));
			    break;
			}
			addr = (addr + size) & addr_mask;
			ioh->out(vm, port, size, val);
		    }
		}
		REGS->ecx = REGS->ecx & ~addr_mask;
		break;
	    }
	default:
	    goto stop;
	}

    case 0x0f:			/* 2-byte escape */
	switch((byte = pop_cs(REGS)))
	{
	case 0x00:		/* Group 6 */
	case 0x01:		/* Group 7 */
	    DB(("vm_gpe: System instruction (i.e. LDGT...)\n"));
	    regs->eip = orig_eip;
	    simulate_vm_int(vm, regs, 6);
	    break;
	default:
	    goto stop;
	}

    default:
    stop:
	regs->eip = orig_eip;
	kprintf("*** VM gpe: pid=%d ec=%x\n", vm->task->pid, REGS->error_code);
	kernel->dump_regs(regs, TRUE);
    }
}

void
vm_invl_handler(struct trap_regs *regs)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);
    u_char byte;

    DB(("vm_invl: vm=%p cs:eip=%x:%x ec=%x\n", vm, REGS->cs, REGS->eip,
	REGS->error_code));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("Invalid opcode exception!\n");
	kernel->dump_regs(regs, TRUE);
    }

    switch((byte = pop_cs(REGS)))
    {
    case 0x63:			/* ARPL Ew,Rw */
	{
	    struct arpl_handler *ah;
	    u_short service = pop_cs(REGS);
	    DB(("vm_invl: ARPL service #0x%x (ah=%02x)\n",
		service, GET8H(regs->eax)));
	    ah = get_arpl_handler(service);
	    if(ah != NULL)
		ah->func(vm, REGS, service);
	    break;
	}

    default:
	/* The invalid opcode exception wasn't introduced until the 186
	   but it shouldn't matter... */
	simulate_vm_int(vm, regs, 6);
    }
}

void
vm_div_handler(struct trap_regs *regs)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);

    DB(("vm_div: vm=%p cs:eip=%x:%x\n", vm, REGS->cs, REGS->eip));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("Divide by zero exception!\n");
	kernel->dump_regs(regs, TRUE);
    }
    else
	simulate_vm_int(vm, regs, 0);
}

void
vm_debug_handler(struct trap_regs *regs)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);

    DB(("vm_debug: vm=%p cs:eip=%x:%x\n", vm, REGS->cs, REGS->eip));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("Debug exception!\n");
	kernel->dump_regs(regs, TRUE);
    }
    else
	simulate_vm_int(vm, regs, 1);
}

void
vm_breakpoint_handler(struct trap_regs *regs)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);

    DB(("vm_breakpoint: vm=%p cs:eip=%x:%x\n", vm, REGS->cs, REGS->eip));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("Breakpoint exception!\n");
	kernel->dump_regs(regs, TRUE);
    }
    else
	simulate_vm_int(vm, regs, 3);
}

void
vm_ovfl_handler(struct trap_regs *regs)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);

    DB(("vm_breakpoint: vm=%p cs:eip=%x:%x\n", vm, REGS->cs, REGS->eip));

    if((REGS->eflags & EFLAGS_VM) == 0)
    {
	kprintf("Overflow exception!\n");
	kernel->dump_regs(regs, TRUE);
    }
    else
	simulate_vm_int(vm, regs, 4);
}

bool
vm_pfl_handler(struct trap_regs *regs, u_long lin_addr)
{
    struct vm *vm = GET_TASK_VM(kernel->current_task);
    DB(("vm_rom: cs:eip=%x:%x\n", REGS->cs, REGS->eip));

    if(regs->error_code & PF_ERROR_PROTECTION)
    {
	/* The only time the top-level pfl handler calls us with a
	   prot error is when it thinks it's a pseudo-ROM page. We attempt
	   to skip the faulting instruction.. */
	if((lin_addr < 0xa0000) || (lin_addr > 0xfffff))
	    return FALSE;
	REGS->eip = SET16(REGS->eip, GET16(REGS->eip)
			  + get_inslen((u_char *)GET16(REGS->eip), TRUE,
			  	       FALSE));
	return TRUE;
    }
    else
    {
	/* Okay; the only other time we get called is when a not-present
	   page was accessed. We can allocate a page and map it in. Note
	   that we're careful to handle the virtual A20 gate.. */
	page *new = kernel->alloc_page();
	page_dir *pd = kernel->current_task->page_dir;
	u_long pte;
	if(!vm->a20_state && (lin_addr >= 0x100000))
	{
	    /* A20 disabled and address above 1M, truncate it back down. */
	    lin_addr -= 0x100000;
	}
	pte = kernel->get_pte(pd, lin_addr);
	kernel->map_page(pd, new, lin_addr & PAGE_MASK,
			 (pte & (PTE_USER | PTE_READ_WRITE | PTE_FREEABLE))
			 | PTE_PRESENT);
	if(!vm->a20_state && (lin_addr < 0x10000))
	{
	    /* A20 disabled, so map the new page above 1M as well. */
	    kernel->map_page(pd, new, (lin_addr & PAGE_MASK) + 0x100000,
			     (pte & (PTE_USER | PTE_READ_WRITE))
			     | PTE_PRESENT);
	}
	return TRUE;
    }
}
