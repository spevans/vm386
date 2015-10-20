/* load.c -- module loader and relocater.
   John Harper. */

#include <vmm/module.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/fs.h>


static struct module *fix_relocs(struct mod_hdr_elf *hdr, struct file *fh, struct module_memory *mem);
static struct fs_module *fs;


static void
free_module_memory(struct module_memory *mem)
{
        if (mem) {
                free(mem->text);
                free(mem->rodata);
                free(mem->data);
                free(mem->bss);
        }
}


/* Allocate the memory for each section. This should really be done
   directly via the page tables so that permissions can be set correctly
   after relocations. */
static struct module_memory *
alloc_module_memory(struct mod_hdr_elf *hdr)
{
        struct module_memory *mem = malloc(sizeof(*mem));
        if (mem != NULL) {
                DB(("Allocating %u bytes for text section\n",
                    hdr->text.size));
                mem->text = malloc(hdr->text.size);
                if (mem->text != NULL) {
                        DB(("Allocating %u bytes for rodata section\n",
                            hdr->rodata.size));
                        mem->rodata = malloc(hdr->rodata.size);
                        if (mem->rodata != NULL) {
                                DB(("Allocating %u+%u bytes for data and bss\n",
                                    hdr->data.size, hdr->bss_size));
                                mem->data = calloc(hdr->data.size + hdr->bss_size, 1);
                                if (mem->data != NULL) {
                                        mem->bss = mem->data + hdr->data.size;
                                        DB(("text = %p/%x rodata = %p/%x data = %p/%x bss = %p/%x\n",
                                            mem->text, hdr->text.size,
                                            mem->rodata, hdr->rodata.size,
                                            mem->data, hdr->data.size,
                                            mem->bss, hdr->bss_size));
                                                return mem;
                                }
                        }
                }
                free_module_memory(mem);
        }
        DB(("Failed to allocate all memory\n"));

        return NULL;
}


static bool
load_section(struct file *fh, void *buf, struct section *section, const char *name)
{
        uint32_t offset = section->offset;
        uint32_t size = section->size;

        long pos = fs->seek(fh, offset, SEEK_ABS);
        if (pos != (long)offset) {
                kprintf("Cant seek to %u for %s section, got %ld\n", offset,
                        name, pos);
                return FALSE;
        }
        long read = fs->read(buf, size, fh);
        if (read != (long)size) {
                kprintf("Cant read %u bytes for %s section, got %ld\n", size,
                        name, read);
                return FALSE;
        }

        return TRUE;
}


static bool
load_sections(struct mod_hdr_elf *hdr, struct module_memory *mem, struct file *fh)
{
        if (load_section(fh, mem->text, &hdr->text, "text")
            && load_section(fh, mem->rodata, &hdr->rodata, "rodata")
            && load_section(fh, mem->data, &hdr->data, "data")) {
                return TRUE;
        } else {
                return FALSE;
        }
}


/* Load the module called NAME from disk into memory, perform any relocations
   necessary and call it's initialisation function, if this returns non-zero
   add the module to the global list. Returns the loaded module or zero. */
struct module *
load_module(const char *name)
{
        struct module *mod = NULL;
        struct file *fh;
        struct mod_hdr_elf hdr;
        char name_buf[strlen(name) + 32];
        struct module_memory *mem = NULL;

        if (fs == NULL) {
                /* Initialiase once */
                fs = (struct fs_module *)open_module("fs", SYS_VER);
        }
        kprintf("Loading `%s.module'\n", name);
        ksprintf(name_buf, "/lib/%s.module", name);
        fh = fs->open(name_buf, F_READ);
        if(fh == NULL) {
                kprintf("Cant open `%s' for reading\n", name_buf);
                goto error;
        }

        long res = fs->read(&hdr, sizeof(hdr), fh);
        if (res != sizeof(hdr)) {
                kprintf("Error loading module, read returned %ld not %d\n",
                        res, sizeof(hdr));
        }

        if (M_BADMAG(hdr) || (hdr.revision != MOD_STRUCT_REV)) {
                kprintf("Bad module header on `%s'\n", name_buf);
                goto error;
        }

        mem = alloc_module_memory(&hdr);
        if (mem == NULL) {
                kprintf("Cant allocate memory for module\n");
                goto error;
        }

        if (!load_sections(&hdr, mem, fh)) {
                kprintf("Cant load sections of module\n");
                goto error;
        }

        mod = fix_relocs(&hdr, fh, mem);
        if (mod == NULL) {
                kprintf("Cant relocate module\n");
                goto error;
        }
        DB(("Initialising module, mod init = %p\n", mod->init));
        kernel->base.open_count++;
        mod->mod_memory = mem;
        mod->mod_size = hdr.text.size;
        mod->is_static = FALSE;
        mod->open_count = -1;
        add_module(mod);

        if(!mod->init || mod->init()) {
                mod->open_count++;
        } else {
                kprintf("Cant init module\n");
                remove_module(mod);
                mod = NULL;
        }

 error:
        fs->close(fh);
        if (mod == NULL) {
                free_module_memory(mem);
        }

        return mod;
}


/* Deallocate all memory associated with the module MOD, basically its code
   and data sections. */
void
free_module(struct module *mod)
{
        if(!mod->is_static) {
                free_module_memory(mod->mod_memory);
                free(mod);
        }
}


static uint32_t
section_size(struct mod_hdr_elf *hdr, enum program_section section)
{
        switch(section) {
        case text_section:   return hdr->text.size;
        case rodata_section: return hdr->rodata.size;
        case data_section:   return hdr->data.size;
        case bss_section:    return hdr->bss_size;
        }

        return 0;
}


static void *
section_memory(struct module_memory *mem, enum program_section section)
{
        switch(section) {
        case text_section:   return mem->text;
        case rodata_section: return mem->rodata;
        case data_section:   return mem->data;
        case bss_section:    return mem->bss;
        }

        return NULL;
}


/* Relocate all of the entries for a given section */
static bool
relocate_section(struct file *fh, struct mod_hdr_elf *hdr, struct section *section,
                 enum program_section psection, struct module_memory *mem)
{
        if (section->reloc_cnt == 0) {
                /* Nothing to do */
                return TRUE;
        }

        bool result = FALSE;
        size_t reloc_size = section->reloc_cnt * sizeof(struct relocation);
        struct relocation *relocations = malloc(reloc_size);
        uint8_t *memory = section_memory(mem, psection); // The memory block to update
        uint32_t mem_size = section_size(hdr, psection); // Size of memory block in bytes

        if (relocations != NULL) {
                long res = fs->read(relocations, reloc_size, fh);
                if (res != (long)reloc_size) {
                        kprintf("Cant read in %u bytes of relocation info got %ld\n",
                                reloc_size, res);
                        goto end;
                }

                for (size_t idx = 0; idx < section->reloc_cnt; idx++) {
                        struct relocation *r = relocations + idx;

                        if (is_relocation(r)) {
                                /* The relocation is an offset to apply to the current
                                   value at the relocation address to add in the start of
                                   the memory section that was allocated. */
                                enum program_section s = relocation_section(r);
                                int32_t s_size = section_size(hdr, s);

                                if (r->value > s_size-4) {
                                        kprintf("value %8X is greater than size %8X\n",
                                                r->value, s_size);
                                        goto end;
                                }
                                if (is_absolute_relocation(r)) {
                                        r->value += (uint32_t)section_memory(mem, s);
                                } else {
                                        /* Relative relocation (ie a call or jmp that
                                           is an offset) */
                                        r->value -= r->offset;
                                }
                        } else {
                                /* Or the relocation is just a symbol address that
                                   needs to be looked up and directly stored -
                                   currently hardcoded just for the `kernel'
                                   module ptr. */
                                r->value = (uint32_t)(&kernel);
                        }

                        if (r->offset > mem_size-4) {
                                kprintf("offset %8X is greater than size %8X\n", r->offset, mem_size);
                                goto end;
                        }

                        /* location is the memory to update */
                        uint32_t *location = (uint32_t *)(memory + r->offset);
                        uint32_t newvalue = *location + r->value;
                        *location = newvalue;
                }
                result = TRUE;
        }

 end:
        free(relocations);
        return result;
}


/* Perform all relocations on the module MOD. HDR was read from the start
   of the module's file, FH. */
static struct module *
fix_relocs(struct mod_hdr_elf *hdr, struct file *fh, struct module_memory *mem)
{
        if (relocate_section(fh, hdr, &hdr->text, text_section, mem) &&
            relocate_section(fh, hdr, &hdr->rodata, rodata_section, mem) &&
            relocate_section(fh, hdr, &hdr->data, data_section, mem)) {
                return (struct module *)(section_memory(mem, hdr->mod_section) +
                                         hdr->mod_offset);
        } else {
                return NULL;
        }
}
