/* objlist.c -- Simple program to print the meta-data in an a.out file
   John Harper. */

#include <stdio.h>
#include <unistd.h>
#include <a.out.h>

static void
print_relocs(int nrels, FILE *f)
{
    printf("\t%16s%16s%10s%10s%10s\n",
	   "r_address", "r_symbolnum", "r_pcrel", "size", "r_extern");
    while(nrels > 0)
    {
	struct relocation_info rel;
	if(fread(&rel, sizeof(rel), 1, f) != 1)
	    return;
	printf("\t%16x%16x%10s%10d%10s\n",
	       rel.r_address, rel.r_symbolnum, rel.r_pcrel ? "t" : "nil",
	       1 << rel.r_length, rel.r_extern ? "t" : "nil");
	nrels--;
    }
}

static void
print_syms(struct exec *x, FILE *f)
{
    int nsyms = x->a_syms / sizeof(struct nlist);
    printf("\t%16s%16s%10s%10s%10s\n",
	   "n_un", "n_type", "n_other", "n_desc", "n_value");
    while(nsyms > 0)
    {
	struct nlist nl;
	if(fread(&nl, sizeof(nl), 1, f) != 1)
	    return;
	printf("\t%16x%16x%10x%10x%10x\n",
	       nl.n_un.n_strx, nl.n_type, nl.n_other, nl.n_desc, nl.n_value);
	nsyms--;
    }
}

int
main(int ac, char **av)
{
    struct exec x;
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
    if(fread(&x, sizeof(x), 1, stdin) != 1)
	goto error;

    printf("Header:\n"
	   "\ta_info=0x%x\n"
	   "\ta_text=0x%x, a_data=0x%x, a_bss=0x%x\n"
	   "\ta_syms=0x%x, a_entry=0x%x\n"
	   "\ta_trsize=0x%x, a_drsize=0x%x\n\n",
	   x.a_info, x.a_text, x.a_data, x.a_bss, x.a_syms, x.a_entry,
	   x.a_trsize, x.a_drsize);

    printf("Symbol table:\n");
    if(fseek(stdin, N_SYMOFF(x), SEEK_SET) != 0)
	goto error;
    print_syms(&x, stdin);

    printf("Text relocs:\n");
    if(fseek(stdin, N_TRELOFF(x), SEEK_SET) != 0)
	goto error;
    print_relocs(x.a_trsize / sizeof(struct relocation_info), stdin);

    printf("Data relocs:\n");
    if(fseek(stdin, N_DRELOFF(x), SEEK_SET) != 0)
	goto error;
    print_relocs(x.a_drsize / sizeof(struct relocation_info), stdin);

    return 0;

error:
    fprintf(stderr, "Error.\n");
    return 10;
}

/*
 * Local Variables:
 * compile-command: "gcc -O2 -N -s objlist.c -o objlist"
 * End:
 */
