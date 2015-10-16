#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#define HEADER_SIZE	32
#define HEADER_MAGIC    0x04100301


void quit(char *s)
{
	fprintf(stderr, s);
	exit(1);
}


uint32_t data(char *buffer, unsigned int idx)
{
        return ((uint32_t *)buffer)[idx];
}


int main(int argc, char *argv[])
{
	FILE *fpin, *fpout;
	char buffer[512];
	uint32_t c,d,b,s;
	int i;
	
	if(argc < 3) {
		quit("usage: bbin infile outfile\n");
	}

	fpin = fopen(argv[1], "r");
	if(fpin == NULL) {
		quit("cant open input file\n");
	}

	fread(&buffer, HEADER_SIZE, 1, fpin);
        uint32_t magic = data(buffer, 0);

        printf("sizeof = %d Magic = %8.8X\n", sizeof(magic), magic);
        if (magic != HEADER_MAGIC)
		quit("bad header - cant find magic number\n");

        uint32_t header_sz = data(buffer, 1);
	if(header_sz != HEADER_SIZE) {
                char buf[64];
                sprintf(buf, "bad header - incorrect size %u != %u\n", header_sz, HEADER_SIZE);
		quit(buf);
        }

	c = data(buffer, 2);
	printf("Code Size: %u bytes\n", c);
	d = data(buffer, 3);
	printf("Data Size: %u bytes\n", d);
	b = data(buffer, 4);
	printf("Bss Size: %u bytes\n", b);
	s = data(buffer, 7);
	printf("Symbol Table Size: %u bytes\n", s);
	if(d|b|s)
		quit("Only a code segment should be present\n");

        fpout = fopen(argv[2], "w");
	if(fpout == NULL) {
		quit("cant open output file\n");
	}

	do {
		memset(buffer, 0, 512);
		i = fread(&buffer, 1, 512, fpin);
		fwrite(&buffer, 1, 512, fpout);
	} while(i == 512);
	fclose(fpin);
	fclose(fpout);

        return 0;
}
