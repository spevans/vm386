/* module.h -- definitions for module handling/calling.
   John Harper. */

#ifndef _VMM_MODULE_H
#define _VMM_MODULE_H

#include <vmm/types.h>

/* A node in the linked list of loaded modules.

   Each module will create its own module structure type which uses this as
   its base type. All exported function pointers and data objects must be
   defined in this new structure. For example,

	struct keyboard_module {
	    struct module base;
	    int         (*create_virtual_keyboard)(int foo, int bar);
	    int		(*switch_keyboard_owner)(struct vm *gainer);
	    ...

   Then in one of the module's object files an instance of this structure
   would be defined something like,

	struct keyboard_module keyboard_module = {
	    MODULE_INIT("keyboard", keyboard_init, keyboard_open,
			keyboard_close, keyboard_expunge),
	    create_virtual_keyboard,
	    switch_keyboard_owner,
	    ...

   The <default.mk> Makefile contains a rule to link a module from a list
   of object files. The `mld' program is used to translate the a.out version
   of the module into a special format (see `struct mod_hdr' below). This
   rules expects each module to contain a data object `FOO_module' where
   `FOO' is the stem of the module's file-name. In the above example
   the module's Makefile would have a rule like:

	keyboard.module : $(OBJS)

   The $(OBJS) would be linked into a single relocatable object file by ld,
   then mld would translate this, looking for a symbol `keyboard_module' (the
   data object defined above).

   Module callers open the module by name, getting a pointer to its structure.
   They can then access any functions/data objects which have been exported
   in the above manner.

   The main benefit of sharing data in this way (as opposed to dynamically
   linking the modules together as they're loaded) is the ease of loading.
   Since all symbol values are known at link-time all the loader has to
   do is fix up all the load-position-relative relocations. (See the
   function load.c:load_module().) It also reduces the size of the module
   files.  */

struct module {
    struct module *next;
    const char	  *name;

    /* The version number of the module. */
    u_short	   version;

    /* The number of outstanding `users' of this module. If it is -1 then
       the module hasn't finished initialising itself and hence can't be
       opened. */
    short	   open_count;

    void	  *mod_start;
    size_t	   mod_size;

    /* When the module is first loaded this function is called (unless it's
       NULL). It should return non-zero if it initialised itself ok, otherwise
       it will be unloaded. Note that at the time this is called the module
       will *not* have been inserted into the kernel's list of loaded
       modules. */
    bool	    (*init)(void);

    /* This function is called each time the module is `opened'; (when another
       module asks for the address of a module). Generally this only needs to
       increment the module's open_count and return a pointer to the module's
       structure.
         If this function is defined as NULL, the default action is taken;
       this is to increment MOD->open_count and return MOD.  */
    struct module * (*open)(void);

    /* Called each time the module MOD is closed. Normally this just
       decrements the module's open_count.
         If this function is NULL the default action, to decrement
       MOD->open_count is taken.  */
    void	    (*close)(struct module *mod);

    /* This function is called when the kernel wants to unload the module. If
       the module's open_count > 0 then it's not possible to expunge and zero
       (false) should be returned. Otherwise the module should deallocate all
       data structures it allocated and return non-zero (true), then the kernel
       will unload the module from memory.
         If this function is NULL the module will never be expunged. */
    bool	    (*expunge)(void);

    bool	      is_static;	/* Module is statically allocated. */
};

/* Builds a module structure definition from its args (each arg initialises
   the structure field of the same name). */
#define MODULE_INIT(name, version, init, open, close, expunge) \
    { NULL, (name), (version), 0, NULL, 0, (init), (open), (close), (expunge) }

/* The current version number of all system modules. */
#define SYS_VER 1


/* How modules are stored in files. */

struct mod_hdr {
    u_short magic;
    u_char revision;
    u_char reserved1;
    u_long init_size;		/* total length of code and data sections */
    u_long bss_size;		/* length of uninitialised data */
    u_long reloc_size;		/* length of relocation information */
    u_char reserved2[32];	/* sizeof(struct mod_hdr) == 48 */
};

struct mod_code_hdr {
    struct module *mod_ptr;		/* module's base structure */
    struct kernel_module **kernel_ptr;	/* module's kernel pointer */
    struct mod_code_hdr *next_mod;	/* the end of this module's code */
};

#define MOD_MAGIC 0xDF41
#define MOD_STRUCT_REV 1

#define M_TXTOFF(mh) (sizeof(struct mod_hdr))
#define M_RELOFF(mh) (M_TXTOFF(mh) + (mh).init_size)
#define M_BADMAG(mh) ((mh).magic != MOD_MAGIC)



#ifdef KERNEL

/* from module.c */
extern bool add_module(struct module *mod);
extern void remove_module(struct module *mod);
extern struct module *find_module(const char *name);
extern struct module *open_module(const char *name, u_short version);
extern void close_module(struct module *mod);
extern bool expunge_module(const char *name);
extern bool add_static_module(struct module *mod);
extern bool init_static_modules(void);
struct shell;
extern void describe_modules(struct shell *sh);
extern struct module *which_module(void *addr);

/* from load.c */
extern struct module *load_module(const char *name);
extern void free_module(struct module *mod);

extern struct mod_code_hdr __first_static_module, __last_static_module;

#endif /* KERNEL */
#endif /* _VMM_MODULE_H */
