/* btoa.c -- Binary to array converter.
   John Harper. */

#include <stdio.h>

int
main(int argc, char **argv)
{
    int index = 0;
    int c;
    if(argc != 2)
    {
	puts("Usage: btoa ARRAY-NAME <SRC >DEST");
	return 1;
    }
    printf("/* Machine generated, *do not edit!* */\n\n"
	   "unsigned char %s[] = {", argv[1]);
    while((c = getchar()) != EOF)
    {
	if((index % 8) == 0)
	    printf("\n\t");
	printf("0x%02X, ", c);
	index++;
    }
    printf("\n};\nint %s_length = %d;\n", argv[1], index);
    return 0;
}
