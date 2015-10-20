/* traps.c  - exception/fault/abort trap handling */

#include <vmm/traps.h>
#include <vmm/kernel.h>
#include <vmm/page.h>
#include <vmm/debug.h>
#include <vmm/tasks.h>
#include <vmm/irq.h>
#include <vmm/vm.h>

void dump_regs(struct trap_regs *regs, bool halt)
{
	struct debug_module *debug;
	forbid();
	if(kernel)
	    debug = (struct debug_module *)find_module("debug");
	else
	    debug = NULL;
	if(regs->eflags & 0x20000)
	{
	    /* VM86, fix the segment registers. */
#define VREGS ((struct vm86_regs *)regs)
	    regs->ds = VREGS->ds;
	    regs->es = VREGS->es;
	    regs->fs = VREGS->fs;
	    regs->gs = VREGS->gs;
	}
	printk("EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n",
		regs->eax, regs->ebx, regs->ecx, regs->edx);
	printk("ESI: %08X EDI: %08X EBP: %08X ESP: %08X\n",
		regs->esi, regs->edi, regs->ebp, regs->esp);
	printk("CS: %04X DS: %04X ES: %04X FS: %04X GS: %04X SS: %04X\n",
		regs->cs, regs->ds, regs->es, regs->fs, regs->gs,
		regs->ss); 
	printk("EIP: %08X EFLAGS %08X\n", regs->eip, regs->eflags);
	if((regs->eflags & EFLAGS_VM) == 0)
	{
	    if(regs->eip >= logical_top_of_kernel)
	    {
		struct module *culprit = which_module((void *)regs->eip);
		if(culprit != NULL)
		{
		    printk("EIP is at offset %08lX of module `%s'\n",
			   regs->eip - (u_long)culprit->mod_memory->text,
			   culprit->name);
		}
	    }
	    if(debug != NULL)
	    {
		debug->ncode((unsigned char *)(regs->eip - 10),
			     (unsigned char *)(regs->eip + 10),
			     regs->eip - 10,
			     FALSE, TRUE);
	    }
	}
	else if(debug != NULL)
	{
	    u_char *pc = (u_char *)((regs->eip & 0xffff) + (regs->cs << 4));
	    debug->ncode(pc - 8, pc + 8, (u_long)(pc - 8),
			 TRUE, FALSE);
	}
#if 1
	if(halt)
	{
	    if(intr_nest_count > 1)
	    {
		printk("In kernel, halting system.\n");
		asm volatile ("cli; hlt");
	    }
	    else
	    {
		printk("Suspending task %lu\n", current_task->pid);
		current_task->flags |= TASK_FROZEN;
		suspend_task(current_task);
	    }
	}
	permit();
#else
	printk("Halting system.\n");
	asm volatile ("cli; hlt");
#endif
}


void divide_error_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[0])
	current_task->exceptions[0](regs);
    else
    {
	printk("divide error!\n");
	dump_regs(regs, TRUE);
    }
}

void debug_exception_handler(struct trap_regs *regs)
{
	if(current_task->exceptions[1])
	    current_task->exceptions[1](regs);
	else
	{
	    int reg = get_dr6() & 15;
	    if(reg != 0)
	    {
		u_long addr;
		if(reg & 1)
		{
		    reg = 0;
		    addr = get_dr0();
		}
		else if(reg & 2)
		{
		    reg = 1;
		    addr = get_dr1();
		}
		else if(reg & 4)
		{
		    reg = 2;
		    addr = get_dr2();
		}
		else
		{
		    reg = 3;
		    addr = get_dr3();
		}
		kprintf("*** Debug exception: cs:eip=%x:%x reg=%d addr=%#lx (dr7=%#08lx)\n",
			regs->cs, regs->eip, reg, addr, get_dr7());
	    }
	    else
		printk("debug exception!\n");
	    dump_regs(regs, TRUE);
	}
	set_dr6(0);
}

void nmi_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[2])
	current_task->exceptions[2](regs);
    else
    {
	printk("NMI!\n");
	dump_regs(regs, TRUE);
    }
}


void single_debug_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[3])
	current_task->exceptions[3](regs);
    else
    {
	printk("Single Step Debug trap!\n");
	dump_regs(regs, TRUE);
    }
}


void overflow_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[4])
	current_task->exceptions[4](regs);
    else
    {
	printk("overflow exception!\n");
	dump_regs(regs, TRUE);
    }
}


void bounds_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[5])
	current_task->exceptions[5](regs);
    else
    {
	printk("bounds limit exception!\n");
	dump_regs(regs, TRUE);
    }
}


void invl_opcode_handler(struct trap_regs *regs)
{	
    if(current_task->exceptions[6])
	current_task->exceptions[6](regs);
    else
    {
	printk("invalid opcode exception!\n");
	dump_regs(regs, TRUE);
    }
}


void dna_handler(struct trap_regs *regs)
{
    if(current_task->exceptions[7])
	current_task->exceptions[7](regs);
    else
    {
	printk("Device Not Available!\n");
	dump_regs(regs, TRUE);
    }
}


void double_fault_handler(struct trap_regs *regs)
{
	printk("Double Fault!\n");
	dump_regs(regs, TRUE);
}


void copro_overrun_handler(struct trap_regs *regs)
{
	printk("co-pro overrun!\n");
	dump_regs(regs, TRUE);
}


void dump_selector(unsigned int sel)
{
	if(sel & 1)
		printk("error occurred whilst processing another exception/interrupt\n");
	if(sel & 2)
		printk("error reading IDT\n");
	printk("selector : %08X in ", sel & ~7);
	if(sel & 4)
		printk("LDT\n");
	else
		printk("GDT\n");
}


void invalid_tss_handler(struct trap_regs *regs)
{
	printk("invalid tss!\n");
	regs->error_code &= 0xFFFF;
	if(!regs->error_code) 
		dump_selector(regs->error_code);
	dump_regs(regs, TRUE);
}


void seg_not_pres_handler(struct trap_regs *regs)
{
	printk("segment not present!\n");
	dump_selector(regs->error_code);
	dump_regs(regs, TRUE);
}


void stack_seg_handler(struct trap_regs *regs)
{
	printk("stack segment exception!\n");
	if(!regs->error_code) 
		printk("Limit violation in stack segment\n");
	else 
		dump_selector(regs->error_code);
	dump_regs(regs, TRUE);
}


void gen_prot_handler(struct trap_regs *regs)
{
	if(current_task->gpe_handler != NULL)
	    current_task->gpe_handler(regs);
	else
	{
	    printk("General Protection violation!\n");
	    printk("Error: %08X\n", regs->error_code);
	    dump_regs(regs, TRUE);
	}
}

#if 0
void page_exception_handler(struct trap_regs *regs)
{
	printk("Page exception!\n");
	printk("Linear address %08X ", get_cr2());
	if(regs->error_code & 1) 
		printk("Page protection violation");
	else
		printk("Page not present");
	printk("  Type: ");
	if(regs->error_code & 2)
		printk("write  ");
	else
		printk("read  ");
	if(regs->error_code & 3)
		printk("User level\n");
	else
		printk("Supervisor level\n");
	dump_regs(regs, TRUE);
}
#endif

void co_pro_err_handler(struct trap_regs *regs)
{
	printk("co-pro error!\n");
	dump_regs(regs, TRUE);
}


void init_trap_gates(void)
{
	set_trap_gate(0, &divide_error);
	set_trap_gate(1, &debug_exception);
	set_trap_gate(2, &nmi);
	set_trap_gate(3, &single_debug);
	set_trap_gate(4, &overflow);
	set_trap_gate(5, &bounds);
	set_trap_gate(6, &invl_opcode);
	set_trap_gate(7, &dna);
	set_trap_gate(8, &double_fault);
	set_trap_gate(9, &copro_overrun);
	set_trap_gate(10, &invalid_tss);
	set_trap_gate(11, &seg_not_pres);
	set_trap_gate(12, &stack_seg);
	set_trap_gate(13, &gen_prot);
	set_trap_gate(14, &page_exception);
	set_trap_gate(16, &co_pro_err);
}
