/* load.c -- module loader and relocater.
   John Harper. */

#include <vmm/module.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/fs.h>


/* Load the module called NAME from disk into memory, perform any relocations
   necessary and call it's initialisation function, if this returns non-zero
   add the module to the global list. Returns the loaded module or zero. */
struct module *
load_module(const char *name)
{
#if 0
    struct fs_module *fs = kernel->get_fs_module();
    struct module *mod = NULL;
    struct file *fh;
    char name_buf[strlen(name) + 32];
    kprintf("Loading `%s.module'\n", name);
    ksprintf(name_buf, "/lib/%s.module", name);
    fh = fs->open(name_buf, F_READ);
    if(fh != NULL)
    {
	struct mod_hdr hdr;
	if((fs->read(&hdr, sizeof(hdr), fh) == sizeof(hdr))
	   && !M_BADMAG(hdr)
	   && (hdr.revision == MOD_STRUCT_REV))
	{
	    size_t mod_size;
	    char *mod_start;
	    mod_size = hdr.init_size + hdr.bss_size;
	    mod_start = malloc(mod_size);
	    if(mod_start != NULL)
	    {
		if((fs->seek(fh, M_TXTOFF(hdr), SEEK_ABS) >= 0)
		   && (fs->read(mod_start, hdr.init_size, fh) > 0))
		{
		    memset(mod_start + hdr.init_size, 0, hdr.bss_size);
		    if(fix_relocs(&hdr, fh, mod_start, mod_size))
		    {
			struct mod_code_hdr *mi = (struct mod_code_hdr *)mod_start;
			mod = mi->mod_ptr;
			*(mi->kernel_ptr) = kernel;
			kernel->base.open_count++;
			mod->mod_start = mod_start;
			mod->mod_size = mod_size;
			mod->is_static = FALSE;
			mod->open_count = -1;
			add_module(mod);
			if(!mod->init || mod->init())
			    mod->open_count++;
			else
			{
			    remove_module(mod);
			    free_module(mod);
			    mod = NULL;
			    kprintf("load_module: can't init mod.\n");
			}
		    }
		}
	    }
	}
	else
	    kprintf("load_module: bad module header\n");
	fs->close(fh);
    }
    else
#endif
	kprintf("load_module: can't open file `%s'\n", name);
    return NULL;
}

/* Deallocate all memory associated with the module MOD, basically its code
   and data sections. */
void
free_module(struct module *mod)
{
    if(!mod->is_static)
    {
	free(mod->mod_start);
	mod->mod_start = NULL;
	mod->mod_size = 0;
    }
}

#if 0
/* Perform all relocations on the module MOD. HDR was read from the start
   of the module's file, FH. */
static bool
fix_relocs(struct mod_hdr *hdr, struct file *fh, char *mod_start,
	   __attribute__((unused)) size_t mod_size)
{
#define RBUFSIZ 64
    size_t i = 0;
    struct fs_module *fs = kernel->get_fs_module();

    if(fs->seek(fh, M_RELOFF(*hdr), SEEK_ABS) >= 0)
    {
	while(i < (hdr->reloc_size / 4))
	{
	    u_long rbuf[RBUFSIZ];
	    int this = min(RBUFSIZ, (hdr->reloc_size / 4) - i);
	    if(fs->read(rbuf, this * 4, fh) >= 0)
	    {
		int j = 0;
		while(j < this)
		    *((u_long *)((u_long)mod_start + rbuf[j++])) += (u_long)mod_start;
		i += j;
	    }
	    else
		return FALSE;
	}
    }
    else
	return FALSE;
    return TRUE;
}
#endif
