/* mdump.c -- Print the contents of a module file.
   John Harper. */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#define __NO_TYPE_CLASHES
#include <vmm/module.h>

static void
print_relocs(int nrels, FILE *f)
{
    printf("%16s\n", "offset");
    while(nrels > 0)
    {
	u_long rel;
	if(fread(&rel, sizeof(rel), 1, f) != 1)
	    return;
	printf("%#16lx\n", rel);
	nrels--;
    }
}

int
main(int ac, char **av)
{
    struct mod_hdr hdr;
    struct mod_code_hdr chdr;
    ac--; av++;
    while((ac >= 1) && (**av == '-'))
    {
	fprintf(stderr, "Error: unknown option, %s\n", *av);
	ac--; av++;
    }
    if(ac >= 1)
    {
	if(!freopen(*av, "rb", stdin))
	{
	    perror("freopen");
	    return 10;
	}
	printf("%s:\n\n", *av);
	ac--; av++;
    }
    else
    {
	if(isatty(fileno(stdin)))
	{
	    fprintf(stderr, "Error: can't take input from a terminal\n");
	    return 10;
	}
	printf("stdin:\n\n");
    }
    if(fseek(stdin, 0, SEEK_SET) != 0)
    {
	fprintf(stderr, "Error: can't use seek on the input stream\n");
	return 10;
    }
    if(fread(&hdr, sizeof(hdr), 1, stdin) != 1)
	goto error;

    printf("Header:\n"
	   "\tmagic=%#x, revision=%#x\n"
	   "\tinit_size=%#lx, bss_size=%#lx, reloc_size=%#lx\n",
	   hdr.magic, hdr.revision, hdr.init_size, hdr.bss_size,
	   hdr.reloc_size);

    if(fread(&chdr, sizeof(chdr), 1, stdin) != 1)
	goto error;
    printf("Code header:\n"
	   "module_struct=%p, kernel_ptr=%p, next_mod=%p\n",
	   chdr.mod_ptr, chdr.kernel_ptr, chdr.next_mod);

    printf("Relocs:\n");
    if(fseek(stdin, M_RELOFF(hdr), SEEK_SET) != 0)
	goto error;
    print_relocs(hdr.reloc_size / 4, stdin);

    return 0;

error:
    fprintf(stderr, "Error.\n");
    return 10;
}
