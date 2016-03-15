#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vmm/fs.h>


#define SECTOR_SIZE     512
// 10MB image with valid C/H/S to satisfy bochs, number of sectors
#define SECTOR_COUNT (16 * 63 * 20)
static const uint32_t image_size = SECTOR_COUNT * SECTOR_SIZE;

// MBR partition table
struct hd_partition {
        uint8_t active_flag;
        uint8_t start_head;
        uint16_t start_cylsec;
        uint8_t system;
        uint8_t end_head;
        uint16_t end_cylsec;
        uint32_t lba_start;
        uint32_t sector_count;
} __attribute__((__packed__));

typedef struct hd_partition hd_partition_t;


int
open_image(char *path, struct stat *stat_buf)
{
        int fd = open(path, O_WRONLY, 0644);
        if (fd < 0) {
                fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd < 0) {
                        perror(path);
                        return -1;
                }
                if (ftruncate(fd, image_size) != 0) {
                        perror("truncate");
                        return -1;
                }

                if (lseek(fd, 0, SEEK_SET) != 0) {
                        perror("lseek");
                        return -1;
                }
        }
        if (fstat(fd, stat_buf) != 0) {
                perror("stat");
                return -1;
        }

        return fd;
}


int main(int argc, char *argv[])
{
        if(argc != 3) {
                fprintf(stderr,
                        "usage: makeimage <mbr> <image>\n");
                return 1;
        }

        FILE *fp = fopen(argv[1], "r");
        if(fp == NULL) {
                fprintf(stderr, "error: cannot open file %s\n", argv[1]);
                return 1;
        }

        char mbr[SECTOR_SIZE];
        if (fread(mbr, SECTOR_SIZE, 1, fp) != 1) {
                perror("reading mbr");
                return 1;
        }
        fclose(fp);

        struct stat stat_buf;
        int fd = open_image(argv[2], &stat_buf);
        if (fd < 0) {
                return 1;
        }

        struct hd_partition *part = (struct hd_partition *)(mbr + PART_TABLE_OFFSET);
        part->active_flag = 0x80; // active partition
        part->start_head = 0;
        part->start_cylsec = 0;
        part->system = VMM_PARTITION_TYPE;
        part->end_head = 0;
        part->end_cylsec = 0;
        part->lba_start = 1;
        part->sector_count = (stat_buf.st_size / SECTOR_SIZE) - part->lba_start;

        if (write(fd, mbr, SECTOR_SIZE) != 512) {
                perror("write mbr");
                close(fd);
                unlink(argv[2]);
                return 1;
        }

        return 0;
}
