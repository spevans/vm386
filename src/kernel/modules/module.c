/* module.c -- module management.
   John Harper. */

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/io.h>
#include <vmm/tasks.h>

/* List of loaded modules. */
static struct module *mod_chain;

/* Link MOD into the global module list. */
bool
add_module(struct module *mod)
{
    forbid();
    mod->next = mod_chain;
    mod_chain = mod;
    permit();
    return TRUE;
}

/* Remove MOD from the global module list. */
void
remove_module(struct module *mod)
{
    struct module **x;
    forbid();
    x = &mod_chain;
    while(*x)
    {
	if(*x == mod)
	{
	    *x = mod->next;
	    break;
	}
	x = &((*x)->next);
    }
    permit();
}

/* Search the module list for one called NAME. Either returns this module,
   or NULL if it can't be found. */
struct module *
find_module(const char *name)
{
    struct module *this;
    forbid();
    this = mod_chain;
    while(this != NULL)
    {
	if(strcmp(this->name, name) == 0)
	    break;
	this = this->next;
    }
    permit();
    return this;
}

/* Open the module called NAME; if this mod hasn't been loaded yet it
   will be. Returns a pointer to the module or NULL. VERSION specifies
   the oldest acceptable version of the module. */
struct module *
open_module(const char *name, u_short version)
{
    struct module *mod;
    forbid();
    mod = find_module(name);
    if(mod == NULL)
    {
	/* Module NAME hasn't been loaded yet. So load it 8-)

	   This uses a semaphore to protect against *other* tasks
	   loading modules at the same time. This task *must* be
	   allowed back in if it wishes. When reading the following
	   code remember the forbid() in action. The only reason
	   this is needed is that load_module() can break the forbid()
           when it calls the filing system.. */

	static struct semaphore load_sem;	/* all zero == clear */
	static struct task *owner_task;

	if(owner_task == current_task)
	    mod = load_module(name);
	else
	{
	    wait(&load_sem);
	    owner_task = current_task;
	    /* In case this module was opened while we slept. */
	    mod = find_module(name);
	    if(mod == NULL)
		mod = load_module(name);
	    owner_task = NULL;
	    signal(&load_sem);
	}
    }
    if(mod != NULL)
    {
	/* If the module's open_count==-1 it's still initialising itself
	   and can't be opened. */
	if((mod->open_count == -1) || (mod->version < version))
	    mod = NULL;
	else if(mod->open)
	    mod = mod->open();
	else
	    mod->open_count++;
    }
    permit();
    return mod;
}

/* Signals to the module MOD that this instance won't be referenced
   anymore. (i.e. it can decrement its open_count). */
void
close_module(struct module *mod)
{
    if(mod->close)
	mod->close(mod);
    else
	mod->open_count--;
}

/* Expunge the module called NAME. This only succeeds if no other modules
   have it open and the module agrees to expunge itself. */
bool
expunge_module(const char *name)
{
    struct module *mod;
    bool rc;
    forbid();
    mod = find_module(name);
    if((mod == NULL) || (mod->open_count == -1))
	rc = TRUE;
    else
    {
	if(mod->expunge && mod->expunge())
	{
	    /* Well, the module agreed to expunge and has deallocated its data.
	       Now its text&data sections can be freed. But first unlink this
	       node from the module list. */
	    remove_module(mod);
	    free_module(mod);
	    kernel->base.open_count--;
	    rc = TRUE;
	}
	else
	    rc = FALSE;
    }
    permit();
    return rc;
}

/* Adds a module which is physically lives in the kernel but is still
   a separate module. This is for stuff like the fs. The kernel will
   have to call this for each module it contains on bootup. Note that
   modules linked into the kernel (by ld) all share the same `kernel'
   pointer (see kernel/kernel_mod.c). */
bool
add_static_module(struct module *mod)
{
    DB(("add_static_module: mod=%p mod->name=%p mod->init=%p\n",
	mod, mod->name, mod->init));
    mod->is_static = TRUE;
    mod->open_count = -1;
    add_module(mod);
    if(!mod->init || mod->init())
    {
	mod->open_count++;
	kernel->base.open_count++;
	return TRUE;
    }
    else
    {
	remove_module(mod);
	return FALSE;
    }
}

/* Initialise all modules linked into the kernel at compile time. */
bool
init_static_modules(void)
{
    struct mod_code_hdr *hdr = &__first_static_module;
    while(hdr != &__last_static_module)
    {
	kprintf("Initialising `%s.module'\n", hdr->mod_ptr->name);
	DB(("hdr=%p hdr->mod=%p hdr->kernel=%p hdr->next=%p\n",
	    hdr, hdr->mod_ptr, hdr->kernel_ptr, hdr->next_mod));
	if(hdr->kernel_ptr != &kernel)
	    *(hdr->kernel_ptr) = kernel;
	hdr->mod_ptr->mod_start = (char *)hdr;
	hdr->mod_ptr->mod_size = (u_long)hdr->next_mod - (u_long)hdr;
 	if(!add_static_module(hdr->mod_ptr))
	{
	    kprintf("Error: can't initialise `%s.module'\n",
		    hdr->mod_ptr->name);
	}
	hdr = hdr->next_mod;
    }
    return TRUE;
}

void
describe_modules(struct shell *sh)
{
    struct module *m = mod_chain;
    sh->shell->printf(sh, "%-10s  %4s  %10s  %10s  %10s  %5s\n",
		      "Module", "Ver", "Base", "Code", "Length", "Users");
    while(m != NULL)
    {
	sh->shell->printf(sh, "%-10s  %4u  %10x  %10x  %10x  %5d\n",
			  m->name, m->version, m, m->mod_start,
			  m->mod_size, m->open_count);
	m = m->next;
    }
}

/* Try to find the module containing the address ADDR in its code section. */
struct module *
which_module(void *addr)
{
    struct module *x;
    forbid();
    x = mod_chain;
    while(x != NULL)
    {
	if((addr >= (void *)x->mod_start)
	   && (addr <= (void *)(x->mod_start + x->mod_size)))
	{
	    break;
	}
	x = x->next;
    }
    permit();
    return x;
}
