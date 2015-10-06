/* cmds.c -- Shell commands to do kernel stuff.
 * John Harper.
 * One or two debug commands added by C.Luke
 */

#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/time.h>
#include <vmm/debug.h>
#include <vmm/segment.h>
#include <vmm/tasks.h>
#include <vmm/traps.h>

extern struct shell_module *shell;

#define DOC_x "x ADDR...\n\
Print the contents of the kernel logical address ADDR (ADDR is in hex)."
int
cmd_x(struct shell *sh, int argc, char **argv)
{
    while(argc >= 1)
    {
	u_long addr = kernel->strtoul(argv[0], NULL, 16);
	shell->printf(sh, "[%#010x] -> %#010x %#06x %#04x %c\n", addr,
                      *(u_long *)addr, *(u_short *)addr, *(u_char *)addr,
                      *(u_char *)addr);
	argc--; argv++;
    }
    return 0;
}

#define DOC_poke "poke ADDR VALUE\n\
Set the double-word at ADDR to VALUE."
int
cmd_poke(__attribute__ ((unused)) struct shell *sh, int argc, char **argv)
{
    u_long *addr, val;
    if(argc < 2)
	return RC_WARN;
    addr = (u_long *)kernel->strtoul(argv[0], NULL, 16);
    val = kernel->strtoul(argv[1], NULL, 16);
    *addr = val;
    return 0;
}

#define DOC_dis "dis [-user] [-16] [-pid X] [START] [LENGTH]\n\
Disassemble LENGTH bytes (or the previous LENGTH argument) starting at\n\
logical address START (or the end of the previous disassembly)."
int
cmd_dis(struct shell *sh, int argc, char **argv)
{
    static u_long start, length;
    bool in_user = FALSE;
    bool is_32bit = TRUE;
    u_long old_cr3 = 0;
    int rc = RC_FAIL;
    if(length == 0)
	length = 32;
    while(argc > 0 && **argv == '-')
    {
	if(!strcmp(*argv, "-user"))
	    in_user = TRUE;
	else if(!strcmp(*argv, "-16"))
	    is_32bit = FALSE;
	else if(!strcmp(*argv, "-pid") && argc > 1)
	{
	    u_long pid = kernel->strtoul(argv[1], NULL, 0);
	    struct task *task = kernel->find_task_by_pid(pid);
	    if(task == NULL)
	    {
		shell->printf(sh, "Error: no task with pid %d\n", pid);
		goto end;
	    }
	    SWAP_CR3(kernel->current_task, TO_PHYSICAL(task->page_dir),
		     old_cr3);
	    argc--; argv++;
	}
	else
	{
	    shell->printf(sh, "Error: unknown option `%s'\n", *argv);
	    goto end;
	}
	argc--; argv++;
    }
    if(argc >= 1)
	start = kernel->strtoul(argv[0], NULL, 16);
    if(argc >= 2)
	length = kernel->strtoul(argv[1], NULL, 16);
    ncode((unsigned char *)start, (unsigned char *)(start + length), (unsigned int)start, in_user, is_32bit);
    start += length;
    rc = 0;
 end:
    if(old_cr3 != 0)
    {
	SWAP_CR3(kernel->current_task,
		 TO_PHYSICAL(kernel->current_task->page_dir),
		 old_cr3);
    }
    return rc;
}

#define DOC_dgdt "dgdt entry\n\
Show the contents of the selector entry where entry is 0,1,2 etc\n"
int
cmd_dgdt(struct shell *sh, int argc, char *argv[])
{
    static char *mem_types[] = {
      "Read-only", "Read-only, accessed", "Read/Write", "Read/Write, accessed",
      "Read-only, expand-down limit", "Read-only, expand-down limit, accessed",
      "Read/Write, expand-down limit", "Read/Write expand-down limit, accessed",
      "Execute-only", "Execute-only, accessed",
      "Execute/Read", "Execute/Read, accessed",
      "Execute-only, conforming", "Execute-only, conforming, accessed",
      "Execute/Read, conforming", "Execute/Read, conforming, accessed"
    };

    static char *sys_types[] = {
      "Undefined", "Available 286 TSS", "LDT", "Busy 286 TSS", "286 Call Gate",
      "Task Gate", "286 Interrupt Gate", "286 Trap Gate", "Undefined",
      "Available 386 TSS", "Undefined", "Busy 386 TSS", "386 Call Gate",
      "Undefined", "386 Interrupt Gate", "386 Trap Gate"
    };

    unsigned long hi,lo;
    int entry;
    unsigned char type;
    desc_table *gdt;

    if(argc < 1) {
        shell->printf(sh, "Error: no entry specified\n");
        return 1;
    }

    gdt = kernel->get_gdt();
    shell->printf(sh, "GDT at %08X\n", gdt);
    entry = kernel->strtoul(argv[0], NULL, 10);
    hi = gdt[entry].hi;
    lo = gdt[entry].lo;

    cli();
    type = (hi >> 8) & 0xff;
    if(type & 16) /* memory seg */ {
        unsigned long limit;
        limit = (lo & 0xffff) | (hi & 0x000f0000);
        shell->printf(sh, "Memory segment: Base: %08X Limit: %08X\n",
		      (lo >> 16) | (hi & 0xff000000), 
		      (hi & 0x00800000) ? (limit << 12) | 0xfff : limit);
        hi = (hi >>8) & 0xffff;
        shell->printf(sh, "Segment Type: %s\n Priv Level %d\t %sPresent %s-bit\n",
		      mem_types[hi & 0xf], (hi >> 5) & 3,
		      (hi & 0x80) ? "" : "Not ", 
		      (hi & 0x4000) ? "32" : "16" );
     } else { /* sys seg */
       int subtype = type & 0xf;
       shell->printf(sh, "System segment: %s ", sys_types[subtype]);
       if(subtype == 9 || subtype == 0xb) {	/* 386 TSS */
           unsigned long base, limit;
           base = (lo >> 16) & 0xffff;
           base |= (hi & 0xff) << 16;
           base |= (hi & 0xff000000);
           limit = lo & 0xffff;
           limit |= (hi & 0x0f0000);
           if(hi & 0x00800000) limit <<= 12;
           shell->printf(sh, "Base: %08X Limit: %08X\n", base, limit);
           shell->printf(sh, "Priv Level %d\t %sPresent\n",
			 (hi >> 13) & 3, (hi & 0x8000) ? "" : "Not ");
        }
     }
     sti();
     return 0;
}

   
#define DOC_dtss "dtss address\n\
dump address as if it were a TSS\n"
int
cmd_dtss(struct shell *sh, int argc, char *argv[])
{
    struct tss *my_tss;
    if(argc < 1) {
        shell->printf(sh, "need an address!\n");
        return 1;
    }
    my_tss = (struct tss *)kernel->strtoul(argv[0], NULL, 16);
    shell->printf(sh, "BackLink = %d\n", my_tss->back_link);
    shell->printf(sh, "SS0: %04X ESP0: %08X  SS1: %04X ESP1: %08X  SS2: %04X ESP2: %08X\n",
		  my_tss->ss0, my_tss->esp0, 
		  my_tss->ss1, my_tss->esp1,
		  my_tss->ss2, my_tss->esp2);
    shell->printf(sh, "EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n",
		  my_tss->eax, my_tss->ebx, my_tss->ecx, my_tss->edx);
    shell->printf(sh, "ESI: %08X  EDI: %08x  EBP: %08X  ESP: %08X\n",
		  my_tss->esi, my_tss->edi, my_tss->ebp, my_tss->esp);
    shell->printf(sh, "CS: %04X  DS: %04X  ES: %04X  FS: %04X  GS: %04X  SS: %04X\n",
		  my_tss->cs, my_tss->ds, my_tss->es, my_tss->fs,
		  my_tss->gs, my_tss->ss);
    shell->printf(sh, "EIP: %08X  EFLAGS: %08X  CR3: %08X\n",
		  my_tss->eip, my_tss->eflags, my_tss->cr3);
    shell->printf(sh, "LDT: %08X  I/O Bitmap: %04X  Trace:%04X\n",
		  my_tss->ldt, my_tss->bitmap, my_tss->trace);
    return 0;
}

#define DOC_dr "dr N ADDR w|rw|x 1|2|4\n\
Set the 386 debug register N to the linear address ADDR."
int
cmd_dr(struct shell *sh, int argc, char **argv)
{
    int reg, type, length;
    u_long addr;
    if(argc == 0)
    {
	shell->printf(sh, "dr0=%08x dr1=%08x dr2=%08x"
		      "dr3=%08x dr7=%08x\n",
		      get_dr0(), get_dr1(), get_dr2(), get_dr3(),
		      get_dr7());
	return 0;
    }
    if(argc < 4)
	return RC_WARN;
    reg = kernel->strtoul(argv[0], NULL, 0);
    addr = kernel->strtoul(argv[1], NULL, 16);
    if(!strcmp("w", argv[2]))
	type = DBUG_WRITE;
    else if(!strcmp("rw", argv[2]))
	type = DBUG_RDWR;
    else if(!strcmp("x", argv[2]))
	type = DBUG_FETCH;
    else
    {
	shell->printf(sh, "Error: unknown type `%s'\n", argv[2]);
	return RC_WARN;
    }
    length = kernel->strtoul(argv[3], NULL, 0);
    if(length != 1 && length != 2 && length != 4)
    {
	shell->printf(sh, "Error: wrong length `%d'\n", length);
	return RC_WARN;
    }
    kernel->set_debug_reg(reg, addr, type, length);
    return 0;
}

#define DOC_d "d [-u] [-p PID] [START] [LENGTH]"
int
cmd_d(struct shell *sh, int argc, char **argv)
{
    bool in_user = FALSE;
    static u_char *start;
    static u_long length = 0x80;
    u_long old_cr3 = 0;
    int rc = RC_FAIL;
    while(argc > 0 && **argv == '-')
    {
	switch(argv[0][1])
	{
	case 'u':
	    in_user = TRUE;
	    break;
	case 'p':
	    if(argc > 1)
	    {
		u_long pid = kernel->strtoul(argv[1], NULL, 0);
		struct task *task = kernel->find_task_by_pid(pid);
		if(task == NULL)
		{
		    shell->printf(sh, "Error: no task with pid %d\n", pid);
		    goto end;
		}
		SWAP_CR3(kernel->current_task, TO_PHYSICAL(task->page_dir),
			 old_cr3);
		argc--; argv++;
	    }
	    else
	    {
		shell->printf(sh, "Error: no PID to `-p' option\n");
		goto end;
	    }
	    break;
	default:
	    shell->printf(sh, "Error: unknown option `%s'\n", *argv);
	    goto end;
	}
	argc--; argv++;
    }
    if(argc > 0)
    {
	start = (u_char *)kernel->strtoul(*argv, NULL, 16);
	argc--; argv++;
    }
    if(argc > 0)
    {
	length = kernel->strtoul(*argv, NULL, 16);
	argc--; argv++;
    }
    if(kernel->check_area(TO_LOGICAL(kernel->current_task->tss.cr3, page_dir *),
			  in_user ? (u_long)start : TO_LINEAR(start), length))
    {
	u_char *x = start;
	u_long len = length;
	while(len != 0)
	{
	    u_long i;
	    shell->printf(sh, "%08x:", x);
	    for(i = 0; i < 16; i++)
	    {
		if(i < len)
		    shell->printf(sh, " %02x", in_user ? get_user_byte(&x[i]) : x[i]);
		else
		    shell->printf(sh, "   ");
	    }
	    shell->printf(sh, "    ");
	    for(i = 0; i < 16; i++)
	    {
		u_char c = (i < len) ? (in_user ? get_user_byte(&x[i]) : x[i]) : ' ';
		shell->printf(sh, "%c", (c < 0x20) ? '.' : c);
	    }
	    shell->printf(sh, "\n");
	    if(len <= 16)
		x += len, len = 0;
	    else
		x += 16, len -= 16;
	}
	start = x;
	rc = 0;
    }
    else
	shell->printf(sh, "Error: address range (%x,%x)is unmapped.\n",
		      start, length);
end:
    if(old_cr3 != 0)
    {
	SWAP_CR3(kernel->current_task,
		 TO_PHYSICAL(kernel->current_task->page_dir),
		 old_cr3);
    }
    return rc;
}

#define DOC_strsrch "strsrch [-c] [-u] [-p pid] [START] [COUNT] [STRING]\n"\
"Search from logical address START for COUNT bytes for the ASCII string \n"\
"STRING.\n"\
"\t-c\tforces a valid-page check on each location before reading it, \n"\
"\t\totherwise it checks for the entire range\n"\
"\t-u\tindicates the supplied address is in the user space\n"\
"\t-p\tindicates the supplied address is in the space of the specified\n"\
"\t\tprocess"
int cmd_strsrch(struct shell *sh, int argc, char **argv)
{
	#define printf shell->printf
	static char *start = NULL, *str = NULL;
	static u_int count = 0x80;
	char *p, f;
	int i, j;
	u_long old_cr3 = 0;
	bool in_user = FALSE, all_chk = FALSE;

	while(argc > 0 && **argv == '-')
	{
		switch(argv[0][1])
		{
		case 'c':
			all_chk = TRUE;
			break;
		case 'u':
			in_user = TRUE;
			break;
		case 'p':
			if(argc > 1)
			{
				u_long pid = kernel->strtoul(argv[1], NULL, 0);
				struct task *task = kernel->find_task_by_pid(pid);
				if(task == NULL)
				{
					printf(sh, "Error: no task with pid %d\n", pid);
					goto end;
				}
				SWAP_CR3(kernel->current_task, TO_PHYSICAL(task->page_dir), old_cr3);
				argc--; argv++;
			}
			else
			{
				printf(sh, "Error: no PID to `-p' option\n");
				goto end;
			}
			break;
		default:
			printf(sh, "Error: unknown option `%s'\n", *argv);
			goto end;
		}
		argc--; argv++;
	}

	if(argc < 3 && (start == NULL || !count || str == NULL))
	{
		printf(sh, "Usage: %s\n", DOC_strsrch);
		goto end;
	}

	if(argc > 0)
	{
		start = (char *)kernel->strtoul(*argv, NULL, 16);
		argc--; argv++;
	}
	if(argc > 0)
	{
		count = kernel->strtoul(*argv, NULL, 16);
		if(!count)
		{
			printf(sh, "Count must be more than 0!\n");
			goto end;
		}
		argc--; argv++;
	}

	if(!all_chk && !kernel->check_area(
		TO_LOGICAL(kernel->current_task->tss.cr3, page_dir *),
		in_user ? (u_long)start : TO_LINEAR(start), count))
	{
		printf(sh, "That range is not enirely mapped.\n");
		goto end;
	}

	if(argc > 0)
	{
		if(str != NULL)
			kernel->free(str);

		for(i=0, j=0; i<argc; i++)
			j += strlen(argv[i]) + 1;

		str = kernel->malloc(j);
		for(i=0, p = str; i<argc; i++)
		{
			strcpy(p, argv[i]);
			p += strlen(argv[i]);
			*p = ' ';
			p++;
		}
		*(p-1) = '\0';
	
		argc = 0; argv = NULL;
	}

	j = strlen(str);
	p = kernel->malloc(j+1);
	f = 0;
	for(i=0; i< (int)count; i++, start++)
	if(!all_chk || (all_chk && kernel->check_area(
		TO_LOGICAL(kernel->current_task->tss.cr3, page_dir *),
		in_user ? (u_long)start : TO_LINEAR(start), count)))
	{
		if(in_user)
			memcpy_from_user(p, start, j);
		else
			memcpy(p, start, j);

		if(!strncmp(p, str, j))
		{
			printf(sh, "%08p  ", start);
			if(!f)
				f = 1;
			else
				f = (f % 8) + 1;
		}
	}
	kernel->free(p);
	if(!f)
	{
		printf(sh, "No matches found.\n");
	}
	else
	{
		if(f == 8)
			printf(sh, "\n");
		printf(sh, "Done.\n");
	}

end:
	if(old_cr3 != 0)
	{
		SWAP_CR3(kernel->current_task,
			 TO_PHYSICAL(kernel->current_task->page_dir),
			 old_cr3);
	}
	return 0;
}

struct shell_cmds debug_cmds =
{
    0,
    { CMD(x), CMD(poke), CMD(dis), CMD(dgdt), CMD(dtss),
      CMD(dr), CMD(d), CMD(strsrch), END_CMD }
};

bool
add_debug_commands(void)
{
    kernel->add_shell_cmds(&debug_cmds);
    return TRUE;
}

void
remove_debug_commands(void)
{
    kernel->remove_shell_cmds(&debug_cmds);
}
