#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *fp_in, *fp_out;
	unsigned char code_buf[480];
	int i;

	if(argc < 3) {
		fprintf(stderr, "usage: %s input.bin output.h\n", argv[0]);
		return 1;
	}

	if((fp_in = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "cant open input file %s\n", argv[1]);
		return 2;
	}
	if((fp_out = fopen(argv[2], "w")) == NULL) {
		fprintf(stderr, "cant open output file %s\n", argv[2]);
		fclose(fp_in);
		return 3;
	}

	if(fread(code_buf, 480, 1, fp_in) != 1) {
		fprintf(stderr, "cant read from file %s\n", argv[1]);
		fclose(fp_in);
		fclose(fp_out);
		return 4;
	}
	fclose(fp_in);

	fprintf(fp_out, "unsigned char bootsect_code[480] = {\n");
	for(i = 0; i < 480; i++) {
		fprintf(fp_out, "0x%2.2X", (unsigned char)code_buf[i]);
		if(i != 479) fprintf(fp_out, ", ");
		if((i % 12) == 11) fprintf(fp_out, "\n");
	}
	fprintf(fp_out, "};\n");
	fclose(fp_out);
	return 0;
}
	



	
