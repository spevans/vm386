#include <stdio.h>

void main()
{
    int line = 0;
    unsigned ch;
    while( (ch = getchar()) != EOF) {
        if(ch == '\n') line++;
        switch(ch) {
        case '\n':
        case '\t':
        case '':
        continue;
        }
        if(ch >= 127 || ch < 32) { 
           printf("Line %d\tControl Code: %d",line,  ch);
           if(ch < 27) printf(" ^%c\n", ch + 'A' -1);
           else printf("\n");
        }
    }
}
