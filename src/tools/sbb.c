#include <stdio.h>
#include <stdlib.h>
#include <vmm/fs.h>
#include <string.h>

#define	BOOT_PARAMS	453

#define BYTE_BOOT_PARAM(x)	*((unsigned char *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define WORD_BOOT_PARAM(x)	*((unsigned short int *)(&bpb.boot_code[BOOT_PARAMS+x]))
#define DWORD_BOOT_PARAM(x)	*((unsigned int *)(&bpb.boot_code[BOOT_PARAMS+x]))

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


int main(int argc, char *argv[])
{
	FILE *bb_file;
	struct boot_blk bpb;

	if(argc < 2) {
		fprintf(stderr, "usage: sbb bootdev\n");
		return 1;
	}

	bb_file = do_open(argv[1]);
	fread(&bpb, 1, FS_BLKSIZ, bb_file);
	fclose(bb_file);
	printf("Boot Device: %02x\n", BYTE_BOOT_PARAM(22));
	printf("Start16:\nCyl+Sec: %04x", WORD_BOOT_PARAM(0));
	printf("\tHead: %02x\tLBA Start: %08x\tLBA Count: %08x\n",
		BYTE_BOOT_PARAM(2), DWORD_BOOT_PARAM(3), DWORD_BOOT_PARAM(7));
	printf("Kernel:\nCyl+Sec: %04x", WORD_BOOT_PARAM(11));
	printf("\tHead: %02x\tLBA Start: %08x\tLBA Count: %08x\n",
		BYTE_BOOT_PARAM(13), DWORD_BOOT_PARAM(14), DWORD_BOOT_PARAM(18));
	
	
	return 0;
}	
