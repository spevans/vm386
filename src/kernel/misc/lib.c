/* lib.c -- Common utility functions. */

#include <vmm/types.h>
#include <vmm/string.h>
#include <vmm/kernel.h>

u_long
strtoul(const char *str, char **ptr, int base)
{
    u_long res = 0;
    u_char c;
    if(*str == 0)
	return 0;
    while(*str == ' ' || *str == '\t')
	str++;
    if(*str == '0')
    {
	str++;
	if(*str)
	{
	    switch(*str)
	    {
	    case 'x': case 'X':
		base = 16;
		str++;
		break;
	    case 'b':
		base = 2;
		str++;
		break;
	    default:
		base = 8;
	    }
	}
	else
	    base = 8;
    }
    else if(base == 0)
	base = 10;
    while((c = *str++))
    {
	int digit;
	if((c >= '0') && (c <= '9'))
	    digit = c - '0';
	else if((c >= 'a') && (c <= 'f'))
	    digit = 10 + (c - 'a');
	else if((c >= 'A') && (c <= 'F'))
	    digit = 10 + (c - 'A');
	else
	{
	    str--;
	    break;
	}
	res = res * base + digit;
    }
    if(ptr != NULL)
	*ptr = (char *)str;
    return res;
}

/* Malloc a buffer large enough to contain the string STR, copy the string
   to it and return a pointer to it, or a null pointer in case of error. */
char *
strdup(const char *str)
{
    size_t length = strlen(str);
    char *ptr = malloc(length + 1);
    if(ptr != NULL)
    {
	memcpy(ptr, str, length);
	ptr[length] = 0;
    }
    return ptr;
}

