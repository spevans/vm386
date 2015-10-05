/* mld.c -- Linker thing for the new module format.
   John Harper.

   Usage: mld [-v] [-v] [-o DEST-FILE] SOURCE-FILE

   This program translates an a.out OMAGIC object file to the format
   required by the kernel's module loader (basically, a header, a single
   hunk containing the module's text and data, plus a table of relocations
   to perform).

   It also performs a number of consistency checks; for example the object
   file may not contain any unresolved external references. Common data
   symbols (global bss objects) are resolved as they are loaded from the
   a.out file.  */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#define __NO_TYPE_CLASHES
#include <vmm/types.h>
#include <vmm/module.h>

struct symbol
{
    u_char *name;
    u_long value;
};

struct mod_obj
{
    struct exec hdr;
    size_t text_size;
    void *text;
    size_t data_size;
    void *data;
    size_t bss_size;
    u_long num_relocs;
    u_long *relocs;
    u_long num_syms;
    struct symbol *syms;
    u_char *strings;
};

#define TEXT_START(m) (0)
#define DATA_START(m) ((m)->text_size + TEXT_START(m))
#define BSS_START(m)  ((m)->data_size + DATA_START(m))

static char verbosity;

#define VB(v, fmt, args...) \
    do { if(verbosity >= (v)) fprintf(stderr, fmt, ## args); } while(0)

static inline void *
xmalloc(size_t len)
{
    void *ptr = malloc(len);
    if(ptr == NULL)
	abort();
    return ptr;
}

static int
read_relocs(struct mod_obj *m, FILE *f, int num_relocs, int dest, char *sect,
	    u_long sect_start)
{
    int i;
    for(i = 0; i < num_relocs; i++)
    {
	struct relocation_info rel;
	if(fread(&rel, sizeof(rel), 1, f) != 1)
	    goto error;
	if(rel.r_length != 2)
	{
	    fprintf(stderr, "Error: reloc size != 4\n");
	    exit(10);
	}
	if(rel.r_pcrel)
	{
	    /* I think that pc-relative relocations should be ignored? They
	       only seem to occur where the linker resolved an inter-object
	       call. There's nothing I can do with them anyway. */
	    VB(2, "mld: ignored pc-relative reloc, offset=%#lx, contents=%#lx\n",
	       m->relocs[dest], *((u_long *)(sect + m->relocs[dest])));
	}
	else
	{
	    m->relocs[dest] = rel.r_address + sect_start;
	    if(rel.r_extern)
	    {
		/* Relocation refers to a symbol. */
		*((u_long *)(sect + rel.r_address)) = m->syms[rel.r_symbolnum].value;
		VB(2, "mld: reloc; offset=%#lx (%s), contents=%#lx\n",
		   m->relocs[dest], m->syms[rel.r_symbolnum].name,
		   *((u_long *)(sect + rel.r_address)));
	    }
	    else
	    {
		VB(2, "mld: reloc: offset=%#lx, contents=%#lx\n",
		   m->relocs[dest], *((u_long *)(sect + rel.r_address)));
	    }
	    dest++;
	}
	/* OK. By now the m->relocs[dest] has the offset of the long-word to
	   be modified and the value at this location will be such that adding
	   the starting address of the text segment to it will correctly
	   perform the relocation. */
    }
    return dest;

error:
    perror(NULL);
    exit(10);
}

static void
read_a_out_obj(struct mod_obj *m, FILE *f, size_t fsiz)
{
    size_t i;

    /* Read the a.out header. */
    if(fseek(f, 0, SEEK_SET) != 0)
	goto error;
    if(fread(&m->hdr, sizeof(m->hdr), 1, f) != 1)
	goto error;

    if(N_MAGIC(m->hdr) != OMAGIC)
    {
	fprintf(stderr, "Error: not an OMAGIC file\n");
	exit(10);
    }

    VB(1, "mld: a_info=%#lx\n", m->hdr.a_info);

    /* Read the text segment. */
    m->text_size = m->hdr.a_text;
    m->text = xmalloc(m->text_size);
    if(fseek(f, N_TXTOFF(m->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(m->text, 1, m->text_size, f) != m->text_size)
	goto error;
    VB(1, "mld: text_size=%d\n", m->text_size);

    /* Read the data segment. */
    m->data_size = m->hdr.a_data;
    m->data = xmalloc(m->data_size);
    if(fseek(f, N_DATOFF(m->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(m->data, 1, m->data_size, f) != m->data_size)
	goto error;
    VB(1, "mld: data_size=%d\n", m->data_size);

    m->bss_size = m->hdr.a_bss;
    VB(1, "mld: initial bss_size=%d\n", m->bss_size);

    m->num_relocs = (m->hdr.a_trsize + m->hdr.a_drsize) / sizeof(struct relocation_info);
    m->relocs = xmalloc(m->num_relocs * 4);
    VB(1, "mld: num_relocs=%ld\n", m->num_relocs);

    /* Allocate memory for symbols and string table, then read the strings. */
    m->num_syms = m->hdr.a_syms / sizeof(struct nlist);
    m->syms = xmalloc(m->num_syms * sizeof(struct symbol));
    VB(1, "mld: num_syms=%ld\n", m->num_syms);
    m->strings = xmalloc(fsiz - N_STROFF(m->hdr));
    if(fseek(f, N_STROFF(m->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(m->strings, 1, fsiz - N_STROFF(m->hdr), f) != (fsiz - N_STROFF(m->hdr)))
	goto error;

    /* Translate the a.out symbol table into my form, fixing all common
       symbols as we go. */
    if(fseek(f, N_SYMOFF(m->hdr), SEEK_SET) != 0)
	goto error;
    for(i = 0; i < m->num_syms; i++)
    {
	struct nlist nl;
	if(fread(&nl, sizeof(nl), 1, f) != 1)
	    goto error;
	m->syms[i].name = nl.n_un.n_strx + m->strings;
	if(nl.n_type == N_EXT)
	{
	    if(nl.n_value == 0)
	    {
		/* Undefined symbol. */
		fprintf(stderr,
			"Error: symbol %s is undefined in this object file\n",
			m->syms[i].name);
		exit(10);
	    }
	    /* A common symbol. Give it some bss space. */
	    m->syms[i].value = m->bss_size + BSS_START(m);
	    m->bss_size = round_to(m->bss_size + nl.n_value, 4);
	    VB(2,
	       "mld: common %s, %#lx (n_type=%#x, n_other=%#x, n_desc=%#x)\n",
	       m->syms[i].name, m->syms[i].value,
	       nl.n_type, nl.n_other, nl.n_desc);
	}
	else
	{
	    /* A normal symbol */
	    m->syms[i].value = nl.n_value;
	    VB(2, "mld: sym %s, %#lx (n_type=%#x, n_other=%#x, n_desc=%#x)\n",
	       m->syms[i].name, m->syms[i].value,
	       nl.n_type, nl.n_other, nl.n_desc);
	}
    }

    /* Now read the relocation tables, fixing them in the process. */
    if(fseek(f, N_TRELOFF(m->hdr), SEEK_SET) != 0)
	goto error;
    i = read_relocs(m, f, m->hdr.a_trsize / sizeof(struct relocation_info),
		    0, m->text, TEXT_START(m));
    if(fseek(f, N_DRELOFF(m->hdr), SEEK_SET) != 0)
	goto error;
    m->num_relocs = read_relocs(m, f,
				m->hdr.a_drsize / sizeof(struct relocation_info),
				i, m->data, DATA_START(m));

    VB(1, "mld: final bss_size=%d\n", m->bss_size);

    return;

error:
    perror(NULL);
    exit(10);
}

#if 0
static struct symbol *
find_symbol(struct mod_obj *m, const char *name)
{
    struct symbol *sym = m->syms;
    int i = 0;
    while(i < m->num_syms)
    {
	if(!strcmp(name, sym->name))
	    return sym;
	i++;
	sym++;
    }
    return NULL;
}
#endif

static void
write_mod_obj(struct mod_obj *m, FILE *dst)
{
    struct mod_hdr hdr;
    hdr.magic = MOD_MAGIC;
    hdr.revision = MOD_STRUCT_REV;
    hdr.init_size = m->text_size + m->data_size;
    hdr.bss_size = m->bss_size;
    hdr.reloc_size = m->num_relocs * 4;
    if(fwrite(&hdr, sizeof(hdr), 1, dst) != 1)
	goto error;
    if(fwrite(m->text, 1, m->text_size, dst) != m->text_size)
	goto error;
    if(fwrite(m->data, 1, m->data_size, dst) != m->data_size)
	goto error;
    if(fwrite(m->relocs, 4, m->num_relocs, dst) != m->num_relocs)
    {
    error:
	perror("fwrite");
        exit(10);
    }
    VB(1, "mld: written mod format object file\n");
}

int
main(int argc, char **argv)
{
    FILE *src = NULL;
    struct mod_obj mod;
    struct stat statb;
    char *dest_file_name = NULL;
    argc--; argv++;
    while((argc >= 1) && (**argv == '-'))
    {
	if(!strcmp(*argv, "-?") || !strcmp(*argv, "-help"))
	{
	    fprintf(stderr,
		    "Usage: mld [-v] [-v] [-o DEST-FILE] SOURCE-FILE\n");
	    exit(1);
	}
	else if(!strcmp(*argv, "-v"))
	    verbosity++;
	else if(!strcmp(*argv, "-o") && (argc >= 2))
	{
	    argc--; argv++;
	    dest_file_name = *argv;
	}
	else
	{
	    fprintf(stderr, "Error: unknown option, %s\n", *argv);
	    exit(5);
	}
	argc--; argv++;
    }
    if(argc >= 1)
    {
	src = fopen(*argv, "rb");
	if(src == NULL)
	{
	    perror("fopen");
	    exit(10);
	}
	argc--; argv++;
    }
    if(src == NULL)
    {
	fprintf(stderr, "Error: no input file\n");
	exit(5);
    }
    if(fstat(fileno(src), &statb) != 0)
    {
	perror("fstat");
	exit(10);
    }
	
    read_a_out_obj(&mod, src, statb.st_size);

    if(dest_file_name != NULL)
    {
	if(!freopen(dest_file_name, "wb", stdout))
	{
	    perror("freopen");
	    exit(10);
	}
	argc--; argv++;
    }

    write_mod_obj(&mod, stdout);

    exit(0);
}
