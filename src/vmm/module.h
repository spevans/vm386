/* module.h -- definitions for module handling/calling.
   John Harper.

   Updated for ELF modules by Simon Evans
 */

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
   of object files. The object files are linked via ld to create FOO_module.o
   which is then linked again using the kernel/modules/module.linker.script
   to merge all of the segments of the same type (ie all .text.* segments become
   the .text segment, same for .rodata and .data).

   The `mld-elf' program is then used to translate the ELF .o module into
   into a special format (see `struct mod_hdr' below). This expects each module
   to contain a data object `FOO_module' where `FOO' is the stem of the module's
   file-name. In the above example the module's Makefile would have a rule like:

	keyboard.module : $(OBJS)

   The $(OBJS) would be linked into a single relocatable object file by ld, then
   linked again with ld to merge the segments then mld-elf would translate this,
   looking for a symbol `keyboard_module' (the data object defined above).

   Module callers open the module by name, getting a pointer to its structure.
   They can then access any functions/data objects which have been exported
   in the above manner.

   The main benefit of sharing data in this way (as opposed to dynamically
   linking the modules together as they're loaded) is the ease of loading.
   Since all symbol values are known at link-time all the loader has to
   do is fix up all the load-position-relative relocations. (See the
   function load.c:load_module().) It also reduces the size of the module
   files.

   The format emitted by mld-elf contains three sections, text, rodata and data
   and relocations for each section. It also allows a symbol lookup although
   this is currently converted to a number (instead of the symbol being a string)
   and currently hardcoded to 0 for the `kernel' module pointer which is the only
   symbol supported.

  */


/* The memory regions allocated for the modules code an data. The BSS is allocated
   as part of the data and is just a pointer into the data section */
struct module_memory {
    void *text;
    void *rodata;
    void *data;
    void *bss;
};


struct module {
    struct module *next;
    const char    *name;

    /* The version number of the module. */
    u_short        version;

    /* The number of outstanding `users' of this module. If it is -1 then
       the module hasn't finished initialising itself and hence can't be
       opened. */
    short          open_count;

    struct module_memory *mod_memory;
    size_t         mod_size;

    /* When the module is first loaded this function is called (unless it's
       NULL). It should return non-zero if it initialised itself ok, otherwise
       it will be unloaded. Note that at the time this is called the module
       will *not* have been inserted into the kernel's list of loaded
       modules. */
    bool            (*init)(void);

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
    void            (*close)(struct module *mod);

    /* This function is called when the kernel wants to unload the module. If
       the module's open_count > 0 then it's not possible to expunge and zero
       (false) should be returned. Otherwise the module should deallocate all
       data structures it allocated and return non-zero (true), then the kernel
       will unload the module from memory.
         If this function is NULL the module will never be expunged. */
    bool            (*expunge)(void);

     /* Module is statically allocated. Set to FALSE by MODULE_INIT but set to
        correct value (TRUE or FALSE) by kernel initialisation or module load. */
    bool              is_static;
};

/* Builds a module structure definition from its args (each arg initialises
   the structure field of the same name). */
#define MODULE_INIT(name, version, init, open, close, expunge) \
        { NULL, (name), (version), 0, NULL, 0,                 \
          (init), (open), (close), (expunge), FALSE }

/* The current version number of all system modules. */
#define SYS_VER 2

/* How modules are stored in files. */
#define MOD_MAGIC 0xDF41
#define MOD_STRUCT_REV 2
#define M_BADMAG(mh) ((mh).magic != MOD_MAGIC)

enum program_section {
    text_section,
    rodata_section,
    data_section,
    bss_section
};

/* Each section has an array of relocation entries to fixup its pointers */
struct relocation {
        uint8_t info;           /* bit 4 type 0 = symbol flag, 1 = relocation
                                   bit 3 absolute/relative relocation
                                   bits 0-2 progam section */
        uint32_t offset;
        int32_t value;          /* Addend for relocation or symbol ID if symbol */
} __attribute__ ((packed));

struct section {
    uint32_t size;              /* object size */
    uint32_t offset;            /* offset into module file to start of object code/data */
    uint32_t reloc_cnt;         /* number of entries */
    uint32_t reloc_off;         /* offset into module file for array of struct relocations */
} __attribute__ ((packed));

struct mod_hdr_elf {
    uint16_t magic;
    uint8_t revision;
    uint8_t reserved1;
    struct section text;
    struct section rodata;
    struct section data;
    uint32_t bss_size;
    enum program_section mod_section;   /* the section containing the module header */
    uint32_t mod_offset;                /* offset into section to module header */
} __attribute__ ((packed));


/* Helper functions to get/set the relocation.info value */
static inline enum program_section relocation_section(struct relocation *r)
{
        return (enum program_section)(r->info & 0x7);
}

static inline bool is_absolute_relocation(struct relocation *r)
{
        return (r->info & 8);
}


static inline bool is_relocation(struct relocation *r)
{
        return (r->info & 16);
}

static inline uint8_t relocation_info(enum program_section section, bool absolute,
                                       bool relocation)
{
        uint8_t result = 0;
        if (relocation) {
                result = 16 | section;
                result |= absolute ? 8 : 0;
        }
        return result;
}


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

#endif /* KERNEL */
#endif /* _VMM_MODULE_H */
