/* mdump.c -- Print the contents of an ELF linked module file.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define __NO_TYPE_CLASHES
#include <vmm/module.h>


static void
usage(void)
{
        fprintf(stderr, "usage: mdump [-d] module\n");
        exit(1);
}


static void
dump_header(struct mod_hdr_elf *hdr)
{
        printf("magic: %4X revision: %d\n", hdr->magic, hdr->revision);
        printf("bss_size: %8.8X\n", hdr->bss_size);

        struct section *section = &hdr->text;
        printf("text:\tsize: %8.8X\toffset: %8.8X reloc cnt: %d offset: %8.8X\n",
               section->size, section->offset, section->reloc_cnt,
               section->reloc_off);

        section = &hdr->rodata;
        printf("rodata:\tsize: %8.8X\toffset: %8.8X reloc cnt: %d offset: %8.8X\n",
               section->size, section->offset, section->reloc_cnt,
               section->reloc_off);

        section = &hdr->data;
        printf("data:\tsize: %8.8X\toffset: %8.8X reloc cnt: %d offset: %8.8X\n",
               section->size, section->offset, section->reloc_cnt,
               section->reloc_off);

        printf("module struct: section: %d offset: %X\n", hdr->mod_section,
               hdr->mod_offset);
}


static void
dump_section(char *name, struct section *section, uint8_t *file)
{
        char ascii[17];
        uint8_t *data = file + section->offset;

        printf("\n%s section\n", name);
        for (size_t off = 0; off < section->size; off++) {
                if (off % 16 == 0) {
                        memset(ascii, 0, 17);
                        printf("%s%8.8zX ", off ? "\n" : "", off);
                }
                uint8_t d = data[off];
                printf("%2.2X ", d);
                ascii[off % 16] = (d >= 0x20 && d <= 0x7e) ? d : '.';
                if (off % 16 == 15) {
                        printf("%s", ascii);
                }
        }
        printf("\n");
}


static const char *
section_name(int16_t section)
{
        switch (section) {
        case 0: return ".text";
        case 1: return ".rodata";
        case 2: return ".data";
        case 3: return ".bss";
        default: return "unknown";
        }
}


static void
dump_relocations(char *name, struct section *section, uint8_t *file)
{

        printf("\n%s relocations\n", name);
        struct relocation *relocations = (struct relocation *)(file + section->reloc_off);
        for (size_t idx = 0; idx < section->reloc_cnt; idx++) {
                struct relocation *r = relocations + idx;
                printf("[%04zu] %8.8X %8.8X %s   %s\n", idx, r->offset, r->value,
                       is_relocation(r) ?
                       (is_absolute_relocation(r) ?  "ABS" : "REL")
                       : "SYM",
                       section_name(relocation_section(r)));
        }
}


int
main(int argc, char **argv)
{
        char *fname = NULL;
        int hexdump = 0;
        struct stat stat_buf;

        argc--;
        argv++;
        while (argc > 0) {
                if (*argv[0] == '-') {
                        if (!strcmp(*argv, "-d")) {
                                hexdump = 1;
                        } else {
                                usage();
                        }
                } else {
                        fname = *argv;
                }
                argc--;
                argv++;
        }

        if (fname == NULL) {
                usage();
        }

        FILE *fp = fopen(fname, "r");
        if (fp == NULL) {
                fprintf(stderr, "Cannot open %s: %s\n", fname, strerror(errno));
                return 1;
        }

        if (fstat(fileno(fp), &stat_buf) < 0) {
                perror("stat");
                return 1;
        }

        uint8_t *file = mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
                             fileno(fp), 0);
        if (file == MAP_FAILED) {
                perror("mmap");
                fclose(fp);
                return 2;
        }
        struct mod_hdr_elf *hdr = (struct mod_hdr_elf *)file;

        dump_header(hdr);
        if (hexdump) {
                dump_section("text", &hdr->text, file);
                dump_section("rodata", &hdr->rodata, file);
                dump_section("data", &hdr->data, file);
        }

        dump_relocations("text", &hdr->text, file);
        dump_relocations("rodata", &hdr->rodata, file);
        dump_relocations("data", &hdr->data, file);

        munmap(file, stat_buf.st_size);
        fclose(fp);

        return 0;
}
