#include <stdio.h>
#include <stdlib.h>
#include <linux/a.out.h>


int main(int argc, char *argv[])
{
	FILE *fpin, *fpout;
	struct exec header;
	char *buf;
	int i;

	if(argc < 3) {
		printf("usage: exe2bin infile outfile\n");
		exit(1);
	}

	fpin = fopen(argv[1], "r");
	if(fpin == NULL) {
		printf("cant open %s\n", argv[1]);
		exit(1);
	}	
	fpout = fopen(argv[2], "w");
	if(fpout == NULL) {
		printf("cant open %s\n", argv[2]);
		exit(1);
	}	
	fread(&header, sizeof(header), 1, fpin);
	printf("Text Segment is %d\n", header.a_text);
	printf("Data Segment is %d\n", header.a_data);
	printf("BSS Segment is %d\n", header.a_bss);
	printf("Symbol Table is %d\n", header.a_syms);
	printf("Total Kernel size is %d bytes %dK\n",
		header.a_text + header.a_data,
		(header.a_text + header.a_data) / 1024);
	printf("Start Address is 0x%8.8X\n", header.a_entry);
	printf("Relocation info for text is %d bytes\n", header.a_trsize);
	printf("Relocation info for data is %d bytes\n", header.a_drsize);
	printf("N_MAGIC = %lo [%s]\n", N_MAGIC(header),
	       N_MAGIC(header) == QMAGIC ? "QMAGIC" :
	       (N_MAGIC(header) == ZMAGIC ? "ZMAGIC" : "?"));
	printf("Offset to text is 0x%8.8X\n", N_TXTOFF(header));
	printf("Offset to data is 0x%8.8X\n", N_DATOFF(header));
	if(N_MAGIC(header) == ZMAGIC)
	{
	    fseek(fpin, N_TXTOFF(header), SEEK_SET);
	    i = header.a_text;
	}
	else if(N_MAGIC(header) == QMAGIC)
	{
	    i = header.a_text - sizeof(header);
	}
	else
	{
	    printf("N_MAGIC(hdr) = %#lo, aborting\n", N_MAGIC(header));
	    exit(10);
	}
#if 0
	i = (i + 4095) & ~4095;
#endif
	buf = (char *)calloc(i, sizeof(char));
	if(buf == NULL) {
		printf("malloc error\n");
		return 1;
	}
	fread(buf, (i > header.a_text) ? header.a_text : i, 1, fpin);
	fwrite(buf, (i > header.a_text) ? header.a_text : i, 1, fpout);
	free(buf);
	fseek(fpin, N_DATOFF(header), SEEK_SET);
	i = (header.a_data + 4095) & ~4095;
	buf = (char *)calloc(i, sizeof(char));
	if(buf == NULL) {
		printf("malloc error\n");
		exit(1);
	}
	fread(buf, header.a_data, 1, fpin);
	fwrite(buf, header.a_data, 1, fpout);
	fclose(fpin);
	fclose(fpout);
	free(buf);
	return 0;
}
