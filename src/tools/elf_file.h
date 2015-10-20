#include <stdio.h>
#include <elf.h>


struct elf_file {
        void *file_data; // mmap()'d input ELF file
        size_t file_len;
        Elf32_Ehdr *elf_hdr;
        Elf32_Shdr *section_headers;
        Elf32_Phdr *program_headers;
        Elf32_Shdr *sh_string_table;
        Elf32_Shdr *string_table;
        Elf32_Shdr *symbol_table;
};

struct elf_file *init_elf_file(FILE *fp);
void deinit_elf_file(struct elf_file *file);
const char *relocation_type(uint8_t type);
char *string_entry(struct elf_file *file, Elf32_Shdr *strtab, unsigned int idx);
char *symbol_name(struct elf_file *file, Elf32_Sym *symbol);
Elf32_Sym *symbol(struct elf_file *file, char *symname);
Elf32_Phdr *program_header(struct elf_file *file, size_t idx);
Elf32_Shdr *section_header(struct elf_file *file, size_t idx);
void *program_data(struct elf_file *file, Elf32_Phdr *header);
char *section_name(struct elf_file *fule, Elf32_Shdr *header);
void dump_elf_header(struct elf_file *file);
void dump_section_header(struct elf_file *file, Elf32_Shdr *header);
void dump_program_header(Elf32_Phdr *hdr);
void dump_relocations(struct elf_file *file, Elf32_Shdr *header);
void dump_symbols(struct elf_file *file);
