/* mld-elf.c -- Linker thing for the new module format. Updated for ELF
   Simon Evans

   Usage: mld-elf [-v] [-v] [-o DEST-FILE] SOURCE-FILE

   This program translates an ELF object file to the format
   required by the kernel's module loader: a header, 3 sections (code,
   rodata, data) and a set of relocations for each section
   hunk containing the module's text and data, plus a table of relocations
   to perform).

   It also performs a number of consistency checks; for example the object
   file may not contain any unresolved external references.

   The modules are passed through the system linker once to merge all of
   the sections of a given type (eg all .text.* -> .text). The Makedefs
   uses this stage to convert the .o to .ko (still elf). mld-elf then converts
   the .ko to .module
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "elf_file.h"

#define __NO_TYPE_CLASHES
#include <vmm/types.h>
#include <vmm/module.h>


struct mod_obj {
        struct elf_file *file;
        char *name;
        struct section text;
        struct section rodata;
        struct section data;
        size_t bss_size;
        uint8_t *text_data;
        uint8_t *rodata_data;
        uint8_t *data_data;
        struct relocation *text_relocations;
        struct relocation *rodata_relocations;
        struct relocation *data_relocations;
};


static char verbosity;


static void
VB(int v, const char *fmt, ...)
{
        if(verbosity >= (v)) {
                va_list args;
                va_start(args, fmt);
                vfprintf(stderr, fmt, args);
                va_end(args);
        }
}


static inline void *
xmalloc(size_t len)
{
        void *ptr = malloc(len);
        if(ptr == NULL) {
                perror("malloc");
                abort();
        }

        return ptr;
}


static inline void *
xrealloc(void *oldptr, size_t newlen)
{
        void *ptr = realloc(oldptr, newlen);
        if (ptr == NULL) {
                perror("realloc");
                abort();
        }

        return ptr;
}


static size_t
align(size_t offset, size_t align)
{
        if (align) {
                align--;
                offset = (offset + align) & ~align;
        }

        return offset;
}


/* For a given section, allocate a buffer and copy the section from the ELF
   file to the buffer */
static void
add_data(struct mod_obj *m, enum program_section section_id, Elf32_Shdr *shdr)
{
        struct section *section = NULL;
        uint8_t **data = NULL;

        switch (section_id) {
        case text_section:
                section = &m->text;
                data = &m->text_data;
                break;

        case rodata_section:
                section = &m->rodata;
                data = &m->rodata_data;
                break;

        case data_section:
                section = &m->data;
                data = &m->data_data;
                break;

        case bss_section:
                return;
        }

        size_t old_size = section->size;
        section->size = align(old_size, shdr->sh_addralign) + shdr->sh_size;
        *data = xrealloc(*data, section->size);
        VB(3, "Copying %lu bytes from to offset %lu\n", section->size, old_size);
        memcpy(*data + old_size, m->file->file_data + shdr->sh_offset, shdr->sh_size);
}


/* Map the ELF section header into a section type */
static enum program_section
get_section(struct elf_file *file, Elf32_Shdr *shdr)
{
        switch(shdr->sh_type) {
        case SHT_NOBITS:
                return bss_section;

        case SHT_PROGBITS:
                // .text
                if (shdr->sh_flags & SHF_EXECINSTR) {
                        return text_section;
                }
                // .data
                else if (shdr->sh_flags & SHF_WRITE) {
                        return data_section;
                }

                // .rodata
                else if (shdr->sh_flags & SHF_ALLOC) {
                        return rodata_section;
                }
                break;

        case SHT_REL:
        case SHT_RELA: {
                size_t idx = shdr->sh_info;
                if (idx < file->elf_hdr->e_shnum) {
                        return get_section(file, file->section_headers + idx);
                }
                break;
        }
        }

        fprintf(stderr, "Unknown program section in relocation\n");
        exit(1);
}


/* Convert all of the relocations in a relocation section into the
   module format relocations. */
static void
add_relocations(struct mod_obj *m, Elf32_Shdr *shdr)
{
        struct elf_file *file = m->file;
        size_t entry_count = shdr->sh_size / shdr->sh_entsize;
        Elf32_Sym *symbols = (Elf32_Sym *)(file->file_data + file->symbol_table->sh_offset);
        Elf32_Rel *relocations = (Elf32_Rel *)(file->file_data + shdr->sh_offset);
        Elf32_Shdr *section_headers = m->file->section_headers;

        struct section *section = NULL;
        struct relocation **relocations_ptr = NULL;
        switch (get_section(file, shdr)) {
        case text_section:
                section = &m->text;
                relocations_ptr = &m->text_relocations;
                break;

        case rodata_section:
                section = &m->rodata;
                relocations_ptr = &m->rodata_relocations;
                break;

        case data_section:
                section = &m->data;
                relocations_ptr = &m->data_relocations;
                break;

        case bss_section:
                fprintf(stderr, "Error: trying to add relocations for bss section!\n");
                exit(1);
                break;
        }

        for (size_t idx = 0; idx < entry_count; idx++) {
                Elf32_Rel *entry = relocations + idx;
                Elf32_Sym *symbol = symbols + ELF32_R_SYM(entry->r_info);
                uint8_t type = ELF32_R_TYPE(entry->r_info);
                VB(3, "type = %s idx = %d\n", relocation_type(type),
                   symbol->st_shndx);

                size_t newsize = sizeof(struct relocation) *
                        (section->reloc_cnt + 1);
                *relocations_ptr = xrealloc(*relocations_ptr, newsize);
                struct relocation *r = (*relocations_ptr + section->reloc_cnt);

                bool absolute_reloc = 0;
                /* Only 2 types of ELF relocation are currently supported */
                if (type == R_386_32) {
                        absolute_reloc = 1;
                } else if (type == R_386_PC32) {
                        absolute_reloc = 0;
                } else {
                        fprintf(stderr, "Unsupported relocation type: `%s'\n",
                                relocation_type(type));
                        exit(1);
                }

                /* external symbol that is exported by kernel, currently only the 'kernel'
                   module symbol is supported. */
                if (symbol->st_shndx == 0) {
                        char *symb = symbol_name(m->file, symbol);
                        VB(3, "symbol = `%s'\n", symb);
                        r->info = relocation_info(0, 0, 0);
                        if (strcmp(symb, "kernel")) {
                                /* TODO expand to handle other symbols,
                                   using r->value as the symbol ID */
                                fprintf(stderr, "Unknown kernel symbol: `%s'\n",
                                        symb);
                                exit(1);
                        }
                        r->value = 0; // Value for 'kernel' symbol
                } else {
                        enum program_section s = get_section(file, section_headers +
                                                             symbol->st_shndx);
                        r->info = relocation_info(s, absolute_reloc, TRUE);
                        r->value = symbol->st_value;
                }
                r->offset = entry->r_offset;
                section->reloc_cnt++;
        }
}


/* Top level conversion of ELF file to the module format */
static void
read_elf_obj(struct mod_obj *m)
{
        if (verbosity > 2) {
                dump_elf_header(m->file);
                printf("[Nr] name\t\t type\tvaddr\t\toffset\tsize\tflags\tlink\tinfo\talign\tentry size\n");
        }

        size_t idx;
        Elf32_Ehdr *elf_hdr = m->file->elf_hdr;
        Elf32_Shdr *section_headers = m->file->section_headers;

        for (idx = 0; idx < elf_hdr->e_shnum; idx++) {
                Elf32_Shdr *shdr = section_headers + idx;
                if (verbosity > 2) {
                        printf("[%2.2d] ", idx);
                        dump_section_header(m->file, section_headers+idx);
                }
                char *name = section_name(m->file, shdr);
                if (strstr(name, ".eh_frame")) {
                        VB(1, "skipping section %s\n", name);
                        continue;
                }
                switch (shdr->sh_type) {
                case SHT_NOBITS: // BSS
                        m->bss_size = shdr->sh_size;
                        VB(1, "Adding %lu bytes of BSS\n", m->bss_size);
                        break;

                case SHT_PROGBITS:
                        if (shdr->sh_size == 0) {
                                VB(1, "Size is 0, skipping section\n");
                                continue;
                        }
                        VB(1, "adding %lu bytes from offset %lu\n",
                           shdr->sh_size, shdr->sh_offset);

                        // .text
                        if (shdr->sh_flags & SHF_EXECINSTR) {
                                VB(1, "Adding to text section\n");
                                add_data(m, text_section, shdr);
                        }

                        // .data
                        else if (shdr->sh_flags & SHF_WRITE) {
                                VB(1, "Adding to data section\n");
                                add_data(m, data_section, shdr);
                        }

                        // .rodata
                        else if (shdr->sh_flags & SHF_ALLOC) {
                                VB(1, "Adding to rodata section\n");
                                add_data(m, rodata_section, shdr);
                        }
                        break;

                case SHT_REL:
                case SHT_RELA:
                        if (verbosity > 2) {
                                dump_relocations(m->file, shdr);
                        }
                        add_relocations(m, shdr);
                        break;
                }
        }

}


/* Look for the struct module in the ELF file, normally a symbol
   called 'FOO_module' and store the section/offset that it points
   to into the module file header */
static bool
update_module_symbol(struct mod_obj *m, struct mod_hdr_elf *hdr)
{
        char symname[strlen(m->name) + 8];

        sprintf(symname, "%s_module", m->name);
        Elf32_Sym *modsym = symbol(m->file, symname);
        if (modsym == NULL) {
                fprintf(stderr, "Cant find symbol `%s'\n", symname);
                return FALSE;
        }
        VB(1, "%s section = %d offset = %08X size=%X\n", symname,
           modsym->st_shndx, modsym->st_value, modsym->st_size);
        if (modsym->st_size < sizeof(struct module)) {
                fprintf(stderr, "%s is the wrong size (%d) should be at least %d\n", symname,
                        modsym->st_size, sizeof(struct module));
                return FALSE;
        }
        Elf32_Shdr *section = section_header(m->file, modsym->st_shndx);
        if (section == NULL) {
                fprintf(stderr, "Cant find section header for section %d\n",
                        modsym->st_shndx);
        }
        hdr->mod_section = get_section(m->file, section);
        hdr->mod_offset = modsym->st_value;

        return TRUE;
}


/* Update the offset and size for each section in the module file's
   header */
void
update_module_header(struct mod_obj *m, struct mod_hdr_elf *hdr)
{
        hdr->magic = MOD_MAGIC;
        hdr->revision = MOD_STRUCT_REV;
        hdr->text = m->text;
        hdr->rodata = m->rodata;
        hdr->data = m->data;
        hdr->bss_size = m->bss_size;

        uint32_t offset = sizeof(struct mod_hdr_elf);
        if (hdr->text.size > 0) {
                hdr->text.offset = offset;
                offset += hdr->text.size;
        }

        if (hdr->rodata.size > 0) {
                hdr->rodata.offset = offset;
                offset += hdr->rodata.size;
        }

        if (hdr->data.size > 0) {
                hdr->data.offset = offset;
                offset += hdr->data.size;
        }

        if (hdr->text.reloc_cnt > 0) {
                hdr->text.reloc_off = offset;
                offset += hdr->text.reloc_cnt * sizeof(struct relocation);
        }

        if (hdr->rodata.reloc_cnt > 0) {
                hdr->rodata.reloc_off = offset;
                offset += hdr->rodata.reloc_cnt * sizeof(struct relocation);
        }

        if (hdr->data.reloc_cnt > 0) {
                hdr->data.reloc_off = offset;
        }
}


/* Write out the header, sections and relocations */
static bool
write_mod_obj(struct mod_obj *m, struct mod_hdr_elf *hdr, FILE *dst)
{
        if(fwrite(hdr, sizeof(*hdr), 1, dst) != 1) {
                perror("Error writing header");
                goto error;
        }

        if (hdr->text.size > 0) {
                assert(ftell(dst) == (long)hdr->text.offset);
                if (fwrite(m->text_data, hdr->text.size, 1, dst) != 1) {
                        perror("Error writing text section");
                        goto error;
                }
        }

        if (hdr->rodata.size > 0) {
                assert(ftell(dst) == (long)hdr->rodata.offset);
                if (fwrite(m->rodata_data, hdr->rodata.size, 1, dst) != 1) {
                        perror("Error writing rodata section");
                        goto error;
                }
        }

        if (hdr->data.size > 0) {
                assert(ftell(dst) == (long)hdr->data.offset);
                if (fwrite(m->data_data, hdr->data.size, 1, dst) != 1) {
                        perror("Error writing data section");
                        goto error;
                }
        }

        if (hdr->text.reloc_cnt > 0) {
                assert(ftell(dst) == (long)hdr->text.reloc_off);
                if (fwrite(m->text_relocations, sizeof(struct relocation),
                           hdr->text.reloc_cnt, dst) != hdr->text.reloc_cnt) {
                        perror("Error writing text relocations");
                        goto error;
                }
        }

        if (hdr->rodata.reloc_cnt > 0) {
                assert(ftell(dst) == (long)hdr->rodata.reloc_off);
                if (fwrite(m->rodata_relocations, sizeof(struct relocation),
                           hdr->rodata.reloc_cnt, dst) != hdr->rodata.reloc_cnt) {
                        perror("Error writing rodata relocations");
                        goto error;
                }
        }

        if (hdr->data.reloc_cnt > 0) {
                assert(ftell(dst) == (long)hdr->data.reloc_off);
                if (fwrite(m->data_relocations, sizeof(struct relocation),
                           hdr->data.reloc_cnt, dst) != hdr->data.reloc_cnt) {
                        perror("Error writing data relocations");
                        goto error;
                }
        }

        printf("mld-elf: written mod format object file\n");
        return TRUE;

 error:
        return FALSE;
}


int
main(int argc, char **argv)
{
        FILE *src = NULL;
        struct mod_obj mod = { 0 };
        struct mod_hdr_elf hdr = { 0 };

        char *dest_file_name = NULL;
        argc--; argv++;
        while((argc >= 1) && (**argv == '-')) {
                if(!strcmp(*argv, "-?") || !strcmp(*argv, "-help")) {
                        fprintf(stderr,
                                "Usage: mld [-v] [-v] MODNAME -o DEST-FILE SOURCE-FILE\n");
                        exit(1);
                }
                else if(!strcmp(*argv, "-v"))
                        verbosity++;

                else if(!strcmp(*argv, "-o") && (argc >= 2)) {
                        argc--; argv++;
                        dest_file_name = *argv;
                }
                else {
                        fprintf(stderr, "Error: unknown option, %s\n", *argv);
                        return 5;
                }
                argc--; argv++;
        }

        if (dest_file_name == NULL) {
                fprintf(stderr, "Missing output filename\n");
                return 2;
        }

        if(argc >= 1) {
                mod.name = *argv;
                argc--; argv++;
        }
        VB(1, "modulename = %s\n", mod.name);

        if(argc >= 1) {
                src = fopen(*argv, "rb");
                if(src == NULL) {
                        perror("Cannot open input file");
                        return 10;
                }
                argc--; argv++;
        }
        if(src == NULL) {
                fprintf(stderr, "Error: no input file\n");
                return 5;
        }

        mod.file = init_elf_file(src);
        if(mod.file == NULL) {
                fprintf(stderr, "Error: cant read ELF file\n");
                return 5;
        }

        if (mod.file->elf_hdr->e_type != ET_REL) {
                fprintf(stderr, "Type (%d) is not a relocatable file\n",
                        mod.file->elf_hdr->e_type);
                return 6;
        }

        read_elf_obj(&mod);
        update_module_header(&mod, &hdr);
        if (!update_module_symbol(&mod, &hdr)) {
                return 7;
        }
        deinit_elf_file(mod.file);

        FILE *output = fopen(dest_file_name, "wb");
        if (output == NULL) {
                perror("Cannot open output file");
                return 10;
        }

        if (!write_mod_obj(&mod, &hdr, output)) {
                fclose(output);
                unlink(dest_file_name);
                return 1;
        } else {
                fclose(output);
        }

        return 0;
}
