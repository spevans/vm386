/* Helper routines for parsing ELF files including funtions
   to dump out some sections */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "elf_file.h"


static int validate_elf_header(struct elf_file *file);
static int find_tables(struct elf_file *file);


static char *program_types[] = { "NULL", "LOAD", "DYNAMIC", "INTERP", "NOTE",
                                 "SHLIB", "PROG HDR", "TLS" };
static const size_t program_types_count = sizeof(program_types) / sizeof(program_types[0]);

static char *reloc_types[] = {
        "R_386_NONE",
        "R_386_32",
        "R_386_PC32",
        "R_386_GOT32",
        "R_386_PLT32",
        "R_386_COPY",
        "R_386_GLOB_DAT",
        "R_386_JMP_SLOT",
        "R_386_RELATIVE",
        "R_386_GOTOFF",
        "R_386_GOTPC",
        "R_386_32PLT",
        "R_386_TLS_TPOFF",
        "R_386_TLS_IE",
        "R_386_TLS_GOTIE",
        "R_386_TLS_LE",
        "R_386_TLS_GD",
        "R_386_TLS_LDM",
        "R_386_16",
        "R_386_PC16",
        "R_386_8",
        "R_386_PC8",
        "R_386_TLS_GD_32",
        "R_386_TLS_GD_PUSH",
        "R_386_TLS_GD_CALL",
        "R_386_TLS_GD_POP",
        "R_386_TLS_LDM_32",
        "R_386_TLS_LDM_PUSH",
        "R_386_TLS_LDM_CALL",
        "R_386_TLS_LDM_POP",
        "R_386_TLS_LDO_32",
        "R_386_TLS_IE_32",
        "R_386_TLS_LE_32",
        "R_386_TLS_DTPMOD32",
        "R_386_TLS_DTPOFF32",
        "R_386_TLS_TPOFF32",
        "R_386_SIZE32",
        "R_386_TLS_GOTDESC",
        "R_386_TLS_DESC_CALL",
        "R_386_TLS_DESC",
        "R_386_IRELATIVE",
};
static const int reloc_types_cnt = sizeof(reloc_types) / sizeof(reloc_types[0]);


const char *
relocation_type(uint8_t type)
{
        if (type < reloc_types_cnt) {
                return reloc_types[type];
        } else {
                return "Unknown";
        }
}


struct elf_file *
init_elf_file(FILE *fp)
{
        struct stat statb;
        int fd = fileno(fp);
        struct elf_file *file = NULL;
        void *data = NULL;

        if (fstat(fd, &statb) != 0) {
                perror("fstat");
                goto error;
        }

        if ((size_t)statb.st_size < sizeof(Elf32_Ehdr)) {
                fprintf(stderr, "File is too small\n");
                goto error;
        }

        data = mmap(NULL, statb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
                perror("mmap failed: ");
                goto error;
        }
        file = calloc(1, sizeof(struct elf_file));
        if (file == NULL) {
                perror("malloc failed: ");
                goto error;
        }
        file->file_data = data;
        file->file_len = statb.st_size;
        file->elf_hdr = (Elf32_Ehdr *)data;
        if (!validate_elf_header(file)) {
                fprintf(stderr, "Invalid ELF header\n");
                goto error;
        }
        if (!find_tables(file)) {
                fprintf(stderr, "Cant find tables\n");
                goto error;
        }

        return file;

 error:
        if (file) {
                free(file);
        }
        if (data) {
                munmap(data, statb.st_size);
        }

        return NULL;
}


void
deinit_elf_file(struct elf_file *file)
{
        munmap(file->file_data, file->file_len);
        free(file);
}


char *
string_entry(struct elf_file *file, Elf32_Shdr *strtab, unsigned int idx)
{
        if (idx >= strtab->sh_size) {
                fprintf(stderr, "Cant find string at idx=%u\n", idx);
                return NULL;
        }

        return file->file_data + strtab->sh_offset + idx;
}


char *
symbol_name(struct elf_file *file, Elf32_Sym *symbol)
{
        char *name = string_entry(file, file->string_table, symbol->st_name);
        if (symbol->st_shndx == SHN_COMMON) {
                return name;
        }
        if (*name == '\0') {
                Elf32_Shdr *section = section_header(file, symbol->st_shndx);
                name = string_entry(file, file->sh_string_table, section->sh_name);
        }

        return name;
}


Elf32_Phdr *
program_header(struct elf_file *file, size_t idx)
{
        return file->program_headers + idx;
}


void *
program_data(struct elf_file *file, Elf32_Phdr *header)
{
        return file->file_data + header->p_offset;
}


void
dump_elf_header(struct elf_file *file)
{
        Elf32_Ehdr *elf_hdr = file->elf_hdr;
        printf("Entry point addr: 0x%8.8X\n", elf_hdr->e_entry);
        printf("Program header table offset: 0x%8.8X\n", elf_hdr->e_phoff);
        printf("Section header table offset: 0x%8.8X\n", elf_hdr->e_shoff);
        printf("Processor flags: 0x%8.8X\n", elf_hdr->e_flags);
        printf("ELF header size: %u (%u)\n", elf_hdr->e_ehsize, sizeof(Elf32_Ehdr));
        printf("Program header table entry size/count: %d/%d\n",
               elf_hdr->e_phentsize, elf_hdr->e_phnum);
        printf("Section header table entry size/count: %d/%d\n",
               elf_hdr->e_shentsize, elf_hdr->e_shnum);
        printf("Section header string table index: %d\n", elf_hdr->e_shstrndx);
}


char *
section_name(struct elf_file *file, Elf32_Shdr *header)
{
        return string_entry(file, file->sh_string_table, header->sh_name);
}


Elf32_Shdr *
section_header(struct elf_file *file, size_t idx)
{
        if (idx > file->elf_hdr->e_shnum) {
                return NULL;
        }

        return file->section_headers + idx;
}


void
dump_section_header(struct elf_file *file, Elf32_Shdr *header)
{
        printf("%-20s", section_name(file, header));
        printf("%u\t", header->sh_type);
        printf("0x%8.8X\t", header->sh_addr);
        printf("%u\t", header->sh_offset);
        printf("%u\t", header->sh_size);
        printf("%u\t", header->sh_flags);
        printf("%u\t", header->sh_link);
        printf("%u\t", header->sh_info);
        printf("%u\t", header->sh_addralign);
        printf("%u\n", header->sh_entsize);
}


void
dump_program_header(Elf32_Phdr *hdr)
{

        char *type = "OTHER";
        char *seg_type = (hdr->p_flags & PF_X) ? "CODE " : "DATA ";

        if (hdr->p_type < program_types_count) {
                type = program_types[hdr->p_type];
        }
        printf("%-20s%-10.8X%-10.8X%-10.8X%-10.8X%-10s%-10.8X\n",
               type, hdr->p_offset, hdr->p_vaddr, hdr->p_filesz,
               hdr->p_memsz, seg_type, hdr->p_align);
}


void
dump_relocations(struct elf_file *file, Elf32_Shdr *header)
{
        char *name = string_entry(file, file->sh_string_table, header->sh_name);
        if (header->sh_type != SHT_REL) {
                fprintf(stderr, "%s is not a relocation section\n", name);
                return;
        }
        char *type_name = "unknown";
        if (header->sh_type == SHT_REL) type_name = "SHT_REL";
        if (header->sh_type == SHT_RELA) type_name = "SHT_RELA";

        printf("\nRelocation section: %s [%s]\n", name, type_name);

        if (header->sh_entsize != sizeof(Elf32_Rel)) {
                fprintf(stderr, "table entry size (%d) is not sizeof(Elf32_Rel) [%d]\n",
                        header->sh_entsize, sizeof(Elf32_Rel));
                return;
        }
        if (header->sh_size % header->sh_entsize) {
                fprintf(stderr, "section size is not a multiple of entrysize (%d/%d)\n",
                        header->sh_size, header->sh_entsize);
                return;
        }

        size_t entry_count = header->sh_size / header->sh_entsize;
        Elf32_Sym *symbols = (Elf32_Sym *)(file->file_data + file->symbol_table->sh_offset);
        Elf32_Rel *relocations = (Elf32_Rel *)(file->file_data + header->sh_offset);
        size_t idx;
        printf("Idx   Name                     value    size offset   section          type\n");
        for (idx = 0; idx < entry_count; idx++) {
                Elf32_Rel *entry = relocations + idx;
                Elf32_Sym *symbol = symbols + ELF32_R_SYM(entry->r_info);

                printf("%4.4d: %-24s %8.8X %4.4X %6X   %-16s %s\n", idx,
                       symbol_name(file, symbol), symbol->st_value,
                       symbol->st_size, entry->r_offset,
                       section_name(file, section_header(file, symbol->st_shndx)),
                       relocation_type(ELF32_R_TYPE(entry->r_info)));
        }
}


Elf32_Sym *
symbol(struct elf_file *file, char *symname)
{
        Elf32_Shdr *symtab = file->symbol_table;
        size_t entry_count = symtab->sh_size / symtab->sh_entsize;
        Elf32_Sym *symbols = (Elf32_Sym *)(file->file_data + symtab->sh_offset);

        for (size_t idx = 0; idx < entry_count; idx++) {
                Elf32_Sym *symbol = symbols+idx;
                if (!strcmp(symname, symbol_name(file, symbol))) {
                        return symbol;
                }
        }

        return NULL;
}


void
dump_symbols(struct elf_file *file)
{
        Elf32_Shdr *symtab = file->symbol_table;
        size_t entry_count = symtab->sh_size / symtab->sh_entsize;
        Elf32_Sym *symbols = (Elf32_Sym *)(file->file_data + symtab->sh_offset);

        printf("Idx   Name                 value    size section\n");
        for (size_t idx = 0; idx < entry_count; idx++) {
                Elf32_Sym *symbol = symbols+idx;

                printf("%4.4d: %-20s %08X %04X %d\n", idx, symbol_name(file, symbol),
                       symbol->st_value, symbol->st_size, symbol->st_shndx);
        }
}


static int
find_tables(struct elf_file *file)
{
        Elf32_Ehdr *elf_hdr = file->elf_hdr;
        file->section_headers = file->file_data + elf_hdr->e_shoff;
        file->program_headers = file->file_data + elf_hdr->e_phoff;
        file->sh_string_table = file->section_headers + elf_hdr->e_shstrndx;
        size_t idx;

        for (idx = 0; idx < file->elf_hdr->e_shnum; idx++) {
                if (idx == file->elf_hdr->e_shstrndx) continue;
                Elf32_Shdr *header = file->section_headers + idx;
                if (header->sh_type == SHT_STRTAB) {
                        file->string_table = header;
                }
                else if (header->sh_type == SHT_SYMTAB) {
                        file->symbol_table = header;
                }
        }
        if (file->string_table == NULL) {
                fprintf(stderr, "Cant find string table\n");
                return 0;
        }
        if (file->symbol_table == NULL) {
                fprintf(stderr, "Cant find symbol table\n");
                return 0;
        }

        return 1;
}


static int
validate_elf_header(struct elf_file *file)
{
        Elf32_Ehdr *elf_hdr = file->elf_hdr;
        unsigned char *e_ident = elf_hdr->e_ident;

        if (memcmp(e_ident, ELFMAG, strlen(ELFMAG))) {
                fprintf(stderr, "Error: not an ELF file\n");
                return 0;
        }

        if (e_ident[EI_CLASS] != ELFCLASS32) {
                fprintf(stderr, "Error: not a 32bit file\n");
                return 0;
        }

        if (e_ident[EI_DATA] != ELFDATA2LSB) {
                fprintf(stderr, "Error: not little endian\n");
                return 0;
        }

        if (e_ident[EI_VERSION] != EV_CURRENT) {
                fprintf(stderr, "Error: version is not current\n");
                return 0;
        }

        if (e_ident[EI_OSABI] != ELFOSABI_SYSV) {
                fprintf(stderr, "Error: OS ABI is not linux\n");
                return 0;
        }

        if (elf_hdr->e_machine != EM_386) {
                fprintf(stderr, "Machine (%d) is not a 386\n", elf_hdr->e_machine);
                return 0;
        }

        if (elf_hdr->e_version != EV_CURRENT) {
                fprintf(stderr, "Version (%d) is not EV_CURRENT\n",
                        elf_hdr->e_version);
                return 0;
        }

        if (sizeof(Elf32_Shdr) != elf_hdr->e_shentsize) {
                fprintf(stderr, "Section header table entry size is wrong (%u != %u)\n",
                        elf_hdr->e_shentsize, sizeof(Elf32_Shdr));
                return 0;
        }

        return 1;
}
