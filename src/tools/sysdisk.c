#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <vmm/fs.h>
#include <linux/hdreg.h>


#define	BOOT_PARAMS	453
#define BYTE_BOOT_PARAM(x)	*((unsigned char *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define WORD_BOOT_PARAM(x)	*((unsigned short int *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define DWORD_BOOT_PARAM(x)	*((unsigned int *)(&bpb.boot_code[BOOT_PARAMS+x]))


int get_info(int fd, struct hd_geometry *geo);


unsigned int get_filelen(FILE *fp)
{
        unsigned int len;

        fseek(fp, 0L, 2);
        len = ftell(fp);
        rewind(fp);
        return len;
}


FILE *do_open(char *file)
{
        FILE *fp;
        fp = fopen(file, "r");
        if(fp == NULL) {
                fprintf(stderr, "error: cannot open file %s\n", file);
                exit(1);
        }
        return fp;
}

void write_out(FILE *in, unsigned int len, FILE *out)
{
        unsigned int round_up = len;
        char *buf;

        round_up += 511;
        round_up &= ~511;

        buf = (char *)calloc(round_up, 1);
        if(buf == NULL) {
                fprintf(stderr, "error: out of memory\n");
                exit(1);
        }

        if(fread(buf, 1, len, in) != len) {
                fprintf(stderr, "error reading file\n");
                exit(1);
        }
        fwrite(buf, 1, round_up, out);
        free(buf);
}


void
usage(void)
{
        fprintf(stderr,
                "usage: sysdisk start16 kernel device [mbrfile]\n");
        exit(1);
}


int main(int argc, char *argv[])
{
        FILE *sys_file, *start16, *kernel;
        unsigned int start16_len, kernel_len;
        unsigned long total_len;
        unsigned int sector_len;
        uint16_t cylsec;
        struct hd_geometry geo;
        struct boot_blk bpb;
        int is_file;

        if(argc != 4) {
                fprintf(stderr, "usage: sysdisk start16 kernel device sys_file\n");
                return 1;
        }

        sys_file = fopen(argv[3], "r+");
        if(sys_file == NULL) {
                fprintf(stderr, "error: cant open %s\n", argv[3]);
                return 1;
        }

        is_file = get_info(fileno(sys_file), &geo);
        // skip the mbr
        fseek(sys_file, is_file ? 512 : 0, SEEK_SET);
        fread(&bpb, 1, FS_BLKSIZ, sys_file);
        if(bpb.magic != FS_BOOT_MAGIC) {
                fprintf(stderr, "%s:bad magic number\n", argv[3]);
                fclose(sys_file);
                return 1;
        }


        printf("magic = %4.4lX\n", bpb.magic);
        printf("total blocks: %lu\ninode bitmap blk: %lu\ninodes blk: %lu\n",
               bpb.sup.total_blocks, bpb.sup.inode_bitmap, bpb.sup.inodes);
        printf("num inodes: %lu\ndata bitmap blk: %lu data blk: %lu\ndata size: %lu\n",
               bpb.sup.num_inodes, bpb.sup.data_bitmap, bpb.sup.data, bpb.sup.data_size);

        start16 = do_open(argv[1]);
        kernel = do_open(argv[2]);

        start16_len = get_filelen(start16);
        kernel_len = get_filelen(kernel);
        total_len = start16_len / 512;
        if(start16_len % 512) total_len++;
        total_len += kernel_len / 512;
        if(kernel_len % 512) total_len++;
        if(total_len >= (bpb.sup.data * (FS_BLKSIZ / 512))) {
                fprintf(stderr,
                "error: insufficient reserved blocks. req: %ld avail %ld\n",
                total_len, bpb.sup.data);
                fclose(sys_file);
                return 1;
        }

        sector_len = (start16_len / 512);
        if(start16_len % 512) sector_len++;

        cylsec = (geo.cylinders - 1) << 6 | geo.sectors;
        printf("LBA: %lu CYL: %X SEC: %X CYL/SEC: %4.4X HEADS: %2.2X\n",
               geo.start, geo.cylinders, geo.sectors, cylsec, geo.heads);

        WORD_BOOT_PARAM(0) = cylsec + (FS_BLKSIZ / 512) -1; /* cyl + sec */
        BYTE_BOOT_PARAM(2) = geo.heads; /* head */
        DWORD_BOOT_PARAM(3) = geo.start + (FS_BLKSIZ / 512); /* lba start */
        DWORD_BOOT_PARAM(7) = sector_len;
        printf("start16 LBA: %u sector length = %u\n", DWORD_BOOT_PARAM(3),
               DWORD_BOOT_PARAM(7));

        DWORD_BOOT_PARAM(14) = geo.start + sector_len + 2;

        sector_len = (kernel_len / 512);
        if(kernel_len % 512) sector_len++;

        WORD_BOOT_PARAM(11) = 0;
        BYTE_BOOT_PARAM(13) = 0;
        DWORD_BOOT_PARAM(18) = sector_len;
        BYTE_BOOT_PARAM(22) = 0x80;
        printf("kernel LBA: %u sector length = %u\n", DWORD_BOOT_PARAM(14),
               DWORD_BOOT_PARAM(18));

        fseek(sys_file, is_file ? 512 : 0, SEEK_SET);
        fwrite(&bpb, 1, FS_BLKSIZ, sys_file);
        write_out(start16, start16_len, sys_file);
        write_out(kernel, kernel_len, sys_file);

        fclose(sys_file);
        fclose(start16);
        fclose(kernel);

        return 0;
}


// returns 1: regular file, 0: blockdev
int
get_info(int fd, struct hd_geometry *geo)
{
        struct stat statbuf = {0};

        if (fstat(fd, &statbuf) != 0) {
                perror("fstat");
                exit(1);
        }

        // Regular file
        if (!S_ISBLK(statbuf.st_mode)) {
                geo->heads = 0;
                geo->sectors = 0;
                geo->cylinders = 1;
                // start of partition is 1 for a file to allow for mbr
                geo->start = 1;
                return 1;
        }

        if (ioctl(fd, HDIO_GETGEO, geo) != 0) {
                perror("ioctl HDIO_GETGEO");
                exit(1);
        }
        printf("heads: %u sectors: %u cyl: %u start: %lu\n", geo->heads,2,4,5L);
        //                       geo->heads, geo->sectors, geo->cylinders, geo->start);
        return 0;
}
