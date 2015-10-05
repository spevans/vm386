/* mstrip.c -- Program to strip all symbols from an a.out object except
   specified symbols.

   Usage: mstrip [-v] [-v] OBJECT-FILE {-s SYMBOL-TO-KEEP} [-o DEST-FILE]

   This is used on a.out format module files to strip all symbols except
   `_kernel' and `_FOO_module' (where FOO is the name of the module). This
   is necessary since modules can be statically linked into the kernel. If
   two separate modules defined a symbol with a particular name there
   would be trouble.

   John Harper. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <a.out.h>
#define __NO_TYPE_CLASHES
#include <vmm/types.h>

struct obj
{
    struct exec hdr;
    u_char *text;
    u_char *data;
    struct relocation_info *trel;
    struct relocation_info *drel;
    struct nlist *syms;
    char *strings;
    size_t strings_size;
};

#define STRPTR(obj, x) ((x) + (obj)->strings)
#define STROFF(obj, x) ((x) - (obj)->strings)

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

static inline void
xfree(void *ptr)
{
    free(ptr);
}

static inline u_long
alloc_bss_object(struct obj *obj, size_t siz)
{
    u_long old = obj->hdr.a_bss;
    obj->hdr.a_bss = round_to(obj->hdr.a_bss + siz, 4);
    return obj->hdr.a_text + obj->hdr.a_data + old;
}

static void
read_obj(struct obj *obj, FILE *f, size_t fsiz)
{
    /* Read the header. */
    if(fseek(f, 0, SEEK_SET) != 0)
	goto error;
    if(fread(&obj->hdr, sizeof(obj->hdr), 1, f) != 1)
	goto error;

    if(N_MAGIC(obj->hdr) != OMAGIC)
    {
	fprintf(stderr, "Error: not an OMAGIC file\n");
	exit(10);
    }

    VB(1, "mstrip: a_info=%#lx\n", obj->hdr.a_info);

    /* Read the text segment. */
    obj->text = xmalloc(obj->hdr.a_text);
    if(fseek(f, N_TXTOFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->text, 1, obj->hdr.a_text, f) != obj->hdr.a_text)
	goto error;
    VB(1, "mstrip: text_size=%d\n", obj->hdr.a_text);

    /* Read the data segment. */
    obj->data = xmalloc(obj->hdr.a_data);
    if(fseek(f, N_DATOFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->data, 1, obj->hdr.a_data, f) != obj->hdr.a_data)
	goto error;
    VB(1, "mstrip: data_size=%d\n", obj->hdr.a_data);

    VB(1, "mstrip: bss_size=%d\n", obj->hdr.a_bss);

    /* Read the relocations. */
    obj->trel = xmalloc(obj->hdr.a_trsize);
    obj->drel = xmalloc(obj->hdr.a_drsize);
    if(fseek(f, N_TRELOFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->trel, 1, obj->hdr.a_trsize, f) != obj->hdr.a_trsize)
	goto error;
    if(fseek(f, N_DRELOFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->drel, 1, obj->hdr.a_drsize, f) != obj->hdr.a_drsize)
	goto error;
    VB(1, "mstrip: a_trsize=%d, a_drsize=%d\n",
       obj->hdr.a_trsize, obj->hdr.a_drsize);

    /* Read symbols and strings. */
    obj->syms = xmalloc(obj->hdr.a_syms);
    if(fseek(f, N_SYMOFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->syms, 1, obj->hdr.a_syms, f) != obj->hdr.a_syms)
	goto error;
    VB(1, "mstrip: a_syms=%d\n", obj->hdr.a_syms);
    obj->strings_size = fsiz - N_STROFF(obj->hdr);
    obj->strings = xmalloc(obj->strings_size);
    if(fseek(f, N_STROFF(obj->hdr), SEEK_SET) != 0)
	goto error;
    if(fread(obj->strings, 1, obj->strings_size, f) != obj->strings_size)
	goto error;
    return;

error:
    fprintf(stderr, "Error.");
    exit(10);
}

static struct nlist *
find_symbol(struct obj *obj, const char *name)
{
    struct nlist *sym = obj->syms;
    int i = obj->hdr.a_syms / sizeof(struct nlist);
    while(--i >= 0)
    {
	if(!strcmp(name, STRPTR(obj, sym->n_un.n_strx)))
	    return sym;
	sym++;
    }
    return NULL;
}

static void
mark_symbol(struct obj *obj, const char *name)
{
    struct nlist *sym = find_symbol(obj, name);
    if(sym == NULL)
    {
	fprintf(stderr, "Error: no symbol %s\n", name);
	exit(10);
    }
    sym->n_other = 1;			/* is this ok? */
    VB(1, "mstrip: Marked symbol %s\n", name);
}

static void
fix_relocs(struct obj *obj, struct relocation_info *relocs, int num_relocs,
	   void *section)
{
    bool errors = FALSE;
    int i;
    for(i = 0; i < num_relocs; i++)
    {
	if(relocs[i].r_extern)
	{
	    struct nlist *sym = &obj->syms[relocs[i].r_symbolnum];
	    if(sym->n_type == N_EXT)
	    {
		if(sym->n_value == 0)
		{
		    /* Undefined external symbol. */
		    fprintf(stderr, "Error: undefined symbol %s\n",
			    STRPTR(obj, sym->n_un.n_strx));
		    errors = TRUE;
		    continue;
		}
		else if(sym->n_other == 1)
		{
		    /* A common symbol that we want to keep. Don't
		       allocate space for it. */
		    continue;
		}
		else
		{
		    /* Common object. Allocate some bss space for it. */
		    sym->n_value = alloc_bss_object(obj, sym->n_value);
		    sym->n_type = N_BSS;
		    relocs[i].r_symbolnum = N_BSS;
		    VB(2, "mstrip: Allocated bss object, %s, %#lx\n",
		       STRPTR(obj, sym->n_un.n_strx), sym->n_value);
		}
	    }
	    else
		relocs[i].r_symbolnum = sym->n_type &= ~N_EXT;
	    relocs[i].r_extern = 0;
	    *(u_long *)(section + relocs[i].r_address) += sym->n_value;
	}
    }
    if(errors)
	exit(10);
}

static void
fix_sym_relocs(struct obj *obj, int old_sym, int new_sym)
{
    int i = 0, max = obj->hdr.a_trsize / sizeof(struct relocation_info);
    while(i < max)
    {
	if(obj->trel[i].r_extern && (obj->trel[i].r_symbolnum == old_sym))
	{
	    obj->trel[i].r_symbolnum = new_sym;
	    VB(2, "mstrip: Fixed text reloc %d\n", i);
	}
	i++;
    }
    max = obj->hdr.a_drsize / sizeof(struct relocation_info);
    i = 0;
    while(i > 0)
    {
	if(obj->drel[i].r_extern && (obj->drel[i].r_symbolnum == old_sym))
	{
	    obj->drel[i].r_symbolnum = new_sym;
	    VB(2, "mstrip: Fixed data reloc %d\n", i);
	}
	i++;
    }
    VB(1, "mstrip: Fixed symbol %d (%s) to %d\n", old_sym,
       STRPTR(obj, obj->syms[old_sym].n_un.n_strx), new_sym);
}

static void
do_strip(struct obj *obj)
{
    /* Any symbols which haven't been marked are discarded. */
    char *new_strings = xmalloc(obj->strings_size);
    struct nlist *new_syms = xmalloc(obj->hdr.a_syms);
    int last_new_string = 4;
    int last_new_sym = 0;
    int i, max = obj->hdr.a_syms / sizeof(struct nlist);
    for(i = 0; i < max; i++)
    {
	if(obj->syms[i].n_other == 1)
	{
	    /* Keep this symbol. */
	    int namlen = strlen(STRPTR(obj, obj->syms[i].n_un.n_strx));
	    new_syms[last_new_sym] = obj->syms[i];
	    new_syms[last_new_sym].n_other = 0;
	    new_syms[last_new_sym].n_un.n_strx = last_new_string;
	    strcpy(new_strings + last_new_string,
		   STRPTR(obj, obj->syms[i].n_un.n_strx));
	    last_new_string += namlen + 1;
	    fix_sym_relocs(obj, i, last_new_sym);
	    last_new_sym++;
	    VB(1, "mstrip: Kept symbol %s\n",
	       STRPTR(obj, obj->syms[i].n_un.n_strx));
	}
	else
	{
	    VB(1, "mstrip: Discarded symbol %s\n",
	       STRPTR(obj, obj->syms[i].n_un.n_strx));
	}
    }
    xfree(obj->syms);
    obj->syms = new_syms;
    obj->hdr.a_syms = last_new_sym * sizeof(struct nlist);
    xfree(obj->strings);
    obj->strings = new_strings;
    obj->strings_size = last_new_string;
    *((u_long *)new_strings) = last_new_string;
}

static void
write_obj(struct obj *obj, FILE *f)
{
    if((fwrite(&obj->hdr, sizeof(obj->hdr), 1, f) != 1)
       || (fwrite(obj->text, 1, obj->hdr.a_text, f) != obj->hdr.a_text)
       || (fwrite(obj->data, 1, obj->hdr.a_data, f) != obj->hdr.a_data)
       || (fwrite(obj->trel, 1, obj->hdr.a_trsize, f) != obj->hdr.a_trsize)
       || (fwrite(obj->drel, 1, obj->hdr.a_drsize, f) != obj->hdr.a_drsize)
       || (fwrite(obj->syms, 1, obj->hdr.a_syms, f) != obj->hdr.a_syms)
       || (fwrite(obj->strings, 1, obj->strings_size, f) != obj->strings_size))
    {
	perror("fwrite");
	exit(10);
    }
    VB(1, "mstrip: Wrote object file.\n");
}

int
main(int argc, char **argv)
{
    char *dest_file_name = NULL, *src_file_name = NULL;
    struct obj obj;
    FILE *dest;
    argc--; argv++;
    while(argc >= 1)
    {
	if(**argv == '-')
	{
	    if(!strcmp(*argv, "-?") || !strcmp(*argv, "-help"))
	    {
		fprintf(stderr, "Usage: mstrip [-v] [-v] OBJ-FILE {-s SYMBOL-TO-KEEP} [-o DEST-FILE]\n");
		exit(1);
	    }
	    else if(!strcmp(*argv, "-v"))
		verbosity++;
	    else if(!strcmp(*argv, "-s") && (argc >= 2))
	    {
		argc--; argv++;
		if(!src_file_name)
		{
		    fprintf(stderr, "Error: no source file\n");
		    exit(10);
		}
		mark_symbol(&obj, *argv);
	    }
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
	}
	else
	{
	    FILE *src = fopen(*argv, "rb");
	    struct stat statb;
	    if(src == NULL)
	    {
		perror(*argv);
		exit(10);
	    }
	    if(fstat(fileno(src), &statb) != 0)
	    {
		perror("fstat");
		exit(10);
	    }
	    read_obj(&obj, src, statb.st_size);
	    fclose(src);
	    src_file_name = *argv;
	}
	argc--; argv++;
    }
    if(!src_file_name)
    {
	fprintf(stderr, "Error: no source file\n");
	exit(10);
    }
    fix_relocs(&obj, obj.trel,
	       obj.hdr.a_trsize / sizeof(struct relocation_info),
	       obj.text);
    fix_relocs(&obj, obj.drel,
	       obj.hdr.a_drsize / sizeof(struct relocation_info),
	       obj.data);
    do_strip(&obj);

    if(dest_file_name == NULL)
	dest_file_name = src_file_name;
    dest = fopen(dest_file_name, "wb");
    if(!dest)
    {
	perror(dest_file_name);
	exit(10);
    }
    write_obj(&obj, dest);
    fclose(dest);
    return 0;
}
