#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


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
                sprintf(buf, "bad header - incorrect size %u != %lu\n", header_sz, HEADER_SIZE);
		quit(buf);
        }

	c = data(buffer, 2);
	printf("Code Size: %lu bytes\n", c);
	d = data(buffer, 3);
	printf("Data Size: %lu bytes\n", d);
	b = data(buffer, 4);
	printf("Bss Size: %lu bytes\n", b);
	s = data(buffer, 7);
	printf("Symbol Table Size: %lu bytes\n", s);
	if(d|b|s)
		quit("Only a code segment should be present\n");
	if(c != 512)
		quit("Code Segment should be 512 bytes\n");

        fpout = fopen(argv[2], "w");
	if(fpout == NULL) {
		quit("cant open output file\n");
	}

	fread(&buffer, 512, 1, fpin);
	fwrite(&buffer, 512, 1, fpout);
	fclose(fpin);
	fclose(fpout);

        return 0;
}
