#include <stdio.h>
#include <stdlib.h>
#include <vmm/fs.h>
#include <string.h>
#include "../fs/bootsect.h"
#define	BOOT_PARAMS	453

#define BYTE_BOOT_PARAM(x)	*((unsigned char *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define WORD_BOOT_PARAM(x)	*((unsigned short int *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define DWORD_BOOT_PARAM(x)	*((unsigned int *)(&bpb.boot_code[BOOT_PARAMS+x]))

void get_info(char *dev, unsigned long *lba,
        unsigned short *cylsec, unsigned char *head);

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
	

int main(int argc, char *argv[])
{
	FILE *sys_file, *start16, *kernel;
	unsigned int start16_len, kernel_len;
	unsigned long total_len;
	unsigned int sector_len;
	unsigned int boot_dev;
        unsigned long lba;
        unsigned short cylsec;
        unsigned char head;
	struct boot_blk bpb;

	if(argc < 5) {
		fprintf(stderr,
			"usage: sysdisk start16 kernel device sys_file\n");
		return 1;
	}

	sys_file = fopen(argv[4], "r+");
	if(sys_file == NULL) {
		fprintf(stderr, "error: cant open %s\n", argv[4]);
		return 1;
	}

	if(argv[3][0] == 'f') {
		boot_dev = atoi(&argv[3][2]);
	} else {
		boot_dev = 128 + (argv[3][2] - 'a');
	}

	get_info(argv[3], &lba, &cylsec, &head);
	printf("Bootdev: %d LBA: %lu CYL/SEC: %4.4X HEADS: %2.2X\n",
		boot_dev, lba, cylsec, head);

	fread(&bpb, 1, FS_BLKSIZ, sys_file);
	if(bpb.magic != FS_BOOT_MAGIC) {
		fprintf(stderr, "%s:bad magic number\n", argv[4]);
		fclose(sys_file);
		return 1;
	}

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


	rewind(sys_file);
	sector_len = (start16_len / 512);
	if(start16_len % 512) sector_len++;

        WORD_BOOT_PARAM(0) = cylsec + (FS_BLKSIZ / 512) -1; /* cyl + sec */
        BYTE_BOOT_PARAM(2) = head; /* head */
        DWORD_BOOT_PARAM(3) = lba + (FS_BLKSIZ / 512); /* lba start */
        DWORD_BOOT_PARAM(7) = sector_len;
        printf("start16 LBA: %lu sector length = %lu\n", DWORD_BOOT_PARAM(3),
               DWORD_BOOT_PARAM(7));

        DWORD_BOOT_PARAM(14) = lba + sector_len + 2;

        sector_len = (kernel_len / 512);
        if(kernel_len % 512) sector_len++;

        WORD_BOOT_PARAM(11) = 0;
        BYTE_BOOT_PARAM(13)= 0;
        DWORD_BOOT_PARAM(18) = sector_len;
        BYTE_BOOT_PARAM(22) = (unsigned char)boot_dev;
        printf("kernel LBA: %lu sector length = %lu\n", DWORD_BOOT_PARAM(14),
               DWORD_BOOT_PARAM(18));

	fwrite(&bpb, 1, FS_BLKSIZ, sys_file);
	write_out(start16, start16_len, sys_file);
	write_out(kernel, kernel_len, sys_file);

	fclose(sys_file);
	fclose(start16);
	fclose(kernel);
	return 0;
}	


#define __PACK__ __attribute__ ((packed))

typedef struct hd_partition {
  unsigned char active_flag __PACK__ ;
  unsigned char start_head __PACK__ ;
  unsigned short start_cylsec __PACK__ ;
  unsigned char system __PACK__ ;
  unsigned char end_head __PACK__ ;
  unsigned short end_cylsec __PACK__ ;
  unsigned long LBA_start __PACK__ ;
  unsigned long part_len __PACK__ ;
} hd_partition_t;



void get_info(char *dev, unsigned long *lba,
        unsigned short *cylsec, unsigned char *head) 
{
  FILE *fp;
  char mbr[512];
  char device[20];
  hd_partition_t *p;
  int i;
  int ext_part = 5;
  int partition;


  if(dev[0] == 'f') {
    *head = 0;
    *cylsec = 1;
    *lba = 0;
    return;
  } else {
    partition = atoi(&dev[3]);
  }
  strcpy(device, "/dev/");
  strcat(device, dev);
  device[strlen(device)-2] = '\0';
  fprintf(stderr, "device = %s\tPartition = %d\n", device, partition);

  fp = fopen(device, "r");
  if(fp == NULL) {
    fprintf(stderr, "fopen failed\n");
    return;
  }
  
  fread(mbr, 512, 1, fp);
  p = (hd_partition_t *)(mbr + 0x1be);
  if(partition < 5) {
    *head = p[partition-1].start_head;
    *cylsec = p[partition-1].start_cylsec;
    *lba = p[partition-1].LBA_start;
    fclose(fp);
    return;
  }
  p = (hd_partition_t *)(mbr + 0x1be);
  for(i = 0; i < 4; i++, p++) {
#if 0
    printf("Partition: %d System: %d LBA: %lu Head: %d CylSec: %d Len: %lu\n",
        i+1, p->system, p->LBA_start, p->start_head, p->start_cylsec, p->part_len);
#endif
    if(p->system == 5) {
        char embr[512];
        unsigned long cur, exto;
        hd_partition_t *ext = p;

        exto = p->LBA_start;
        cur = exto;
        while(1) {
                rewind(fp);
                fseek(fp, cur * 512, SEEK_CUR);
                if(!fread(embr, 512, 1, fp)) {
                        fprintf(stderr, "fread failed\n");
                        return;
                }
                ext = (hd_partition_t *)(embr + 0x1be);
#if 0
                printf("Partition: %d System: %d LBA: %lu Head: %d CylSec: %d Len: %lu\n",
                        ext_part, ext->system, ext->LBA_start + cur,
                        ext->start_head, ext->start_cylsec, ext->part-len);
#endif
                if(partition == ext_part) {
                        *head = ext->start_head;
                        *cylsec = ext->start_cylsec;
                        *lba = ext->LBA_start + cur;
                        fclose(fp);
                        return;
                }
                ext++;
                ext_part++;
                cur = exto + ext->LBA_start;
                if(ext->system != 5) break;
        }
    }
  }
  fclose(fp);
}
