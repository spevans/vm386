#include <stdio.h>
#include <stdlib.h>

#define HEADER_SIZE	32

void quit(char *s)
{
	fprintf(stderr, s);
	exit(1);
}


void main(int argc, char *argv[])
{
	FILE *fpin, *fpout;
	char buffer[512];
	long c,d,b,s;
	
	if(argc < 3) {
		quit("usage: bbin infile outfile\n");
	}

	fpin = fopen(argv[1], "r");
	if(fpin == NULL) {
		quit("cant open input file\n");
	}	
	fpout = fopen(argv[2], "w");
	if(fpout == NULL) {
		quit("cant open output file\n");
	}	
	fread(&buffer, HEADER_SIZE, 1, fpin);
	if(((long *) buffer)[0] != (0x04100301))
		quit("bad header\n");
	if(((long *) buffer)[1] != (HEADER_SIZE))
		quit("bad header\n");
	c = ((long *) buffer)[2];
	printf("Code Size: %lu bytes\n", c);
	d = ((long *) buffer)[3];
	printf("Data Size: %lu bytes\n", d);
	b = ((long *) buffer)[4];
	printf("Bss Size: %lu bytes\n", b);
	s = ((long *) buffer)[7];
	printf("Symbol Table Size: %lu bytes\n", s);
	if(d|b|s)
		quit("Only a code segment should be present\n");
	if(c != 512)
		quit("Code Segment should be 512 bytes\n");
	fread(&buffer, 512, 1, fpin);
	fwrite(&buffer, 512, 1, fpout);
	fclose(fpin);
	fclose(fpout);
}
