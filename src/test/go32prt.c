/* go32prt.c -- printf() for DJGPP
   John Harper. */

#include <go32.h>
#include <pc.h>

#define __NO_TYPE_CLASHES
#include <vmm/types.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <test/go32prt.h>

/* All functions in this file are borrowed from shell/io.c.  They're
   needed because go32 switches back to real mode to execute a printf()
   statement: that isn't what I want. */

#define SCR_WIDTH 80
#define SCR_HEIGHT 25
#define SCR_BASE ((char *)ScreenPrimary)

/* Coordinates of the cursor. */
static int cursor_x, cursor_y;

/* Scroll the video buffer one line upwards. */
static inline void
scroll_screen_up(void)
{
    /* This should (hopefully) scroll the contents of the screen buffer
       up one row then blank the exposed bottom row. */
    asm volatile ("cld\n\t"
		  "rep\n\t"
		  "movsw\n\t"
		  "movl %0,%%ecx\n\t"
		  "movw $0x0720,%%ax\n\t"
		  "rep\n\t"
		  "stosw"
		  : /* no output */
		  : "i" (SCR_WIDTH),
		    "c" (SCR_WIDTH * (SCR_HEIGHT - 1)),
		    "S" (SCR_BASE + (SCR_WIDTH * 2)),
		    "D" (SCR_BASE)
		  : "ax", "ecx", "si", "di", "memory");
}

/* Clears the whole screen. Stores the SPC character with the standard
   attribute in each cell. */
static inline void
clear_screen(void)
{
    asm volatile ("cld\n\t"
		  "rep\n\t"
		  "stosw"
		  : /* no output */
		  : "a" (0x0720),
		    "c" (SCR_WIDTH * SCR_HEIGHT),
		    "D" (SCR_BASE)
		  : "di", "memory");
}

/* Set the cursor to (X,Y) */
static void
set_cursor_pos(int x, int y)
{
    unsigned short offset;
    if((x > SCR_WIDTH) || (y > SCR_HEIGHT))
	return;
    cursor_x = x;
    cursor_y = y;
    offset = (y * SCR_WIDTH) + x;
    outb(14, 0x3d4);
    outb(offset >> 8, 0x3d5);
    outb(15, 0x3d4);
    outb(offset & 0xff, 0x3d5);
}

/* Set the character under the cursor to C. */
static inline void
set_char(char c)
{
    SCR_BASE[(cursor_y * SCR_WIDTH * 2) + (cursor_x * 2)] = c;
    SCR_BASE[(cursor_y * SCR_WIDTH * 2) + (cursor_x * 2) + 1] = 7;
}

/* Print LENGTH characters from the string TEXT. */
void
print(const char *text, size_t length)
{
    char *cursor_char = SCR_BASE + (cursor_y * SCR_WIDTH * 2) + (cursor_x * 2);
    char c;
    while(length-- > 0)
    {
	c = *text++;
	if((c == '\n') || (cursor_x >= SCR_WIDTH))
	{
	    cursor_x = 0;
	    if(++cursor_y == SCR_HEIGHT)
	    {
		scroll_screen_up();
		cursor_y--;
	    }
	    cursor_char = (SCR_BASE + (cursor_y * SCR_WIDTH * 2)
			   + (cursor_x * 2));
	    if(c == '\n')
		continue;
	}
	*cursor_char++ = c;
	*cursor_char++ = 0x07;
	cursor_x++;
    }
    set_cursor_pos(cursor_x, cursor_y);
}

bool
setup_screen(void)
{
    clear_screen();
    set_cursor_pos(0, 0);
    return TRUE;
}


/* printf() code. Ripped from kernel/printf.c */

static char printf_buf[1024];

#define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

/* Flags */
#define PF_MINUS 1
#define PF_PLUS  2
#define PF_SPC   4
#define PF_HASH  8
#define PF_ZERO  16
#define PF_SHORT 32
#define PF_LONG  64

/* Syntax of format specifiers in FMT is `% [FLAGS] [WIDTH] [TYPE] CONV'
   (without the spaces). FLAGS can be any of:

	`-' Left justify the contents of the field.
	`+' Put a plus character in front of positive signed integers.
	` ' Put a space in from of positive signed integers (only if no `+').
	`#' Put `0' before octal numbers and `0x' or `0X' before hex ones.
	`0' Pad right-justified fields with zeros, not spaces.

   WIDTH is a decimal integer defining the field width (for justification
   purposes). Right justification is the default, use the `-' flag to
   change this.

   TYPE is an optional type conversion for integer arguments, it can be
   either `h' to specify a `short int' argument or `l' to specify a `long
   int' argument.

   CONV defines how to print the argument, it can be one of:

	`i', `d'	A signed decimal integer.
	`u', `Z'	An unsigned decimal integer.
	`b'		Unsigned binary integer.
	`o'		A signed octal integer.
	`x', `X'	An unsigned hexadecimal integer, `x' gets lower-
			case hex digits, `X' upper.
	`p'		A pointer, printed in hexadecimal with a preceding
			`0x' (i.e. like `%#x') unless the pointer is NULL
			when `(nil)' is printed.
	`c'		A character.
	`s'		A string.  */

/* If you're a goto-purist please avert your eyes.. There's more goto
   statements in this function than in every other piece of code I've
   ever written 8-) */

void
kvsprintf(char *buf, const char *fmt, va_list args)
{
    char c, *orig = buf;
    while((c = *fmt++) != 0)
    {
	if(c != '%')
	    *buf++ = c;
	else
	{
	    if(*fmt != '%')
	    {
		int flags = 0;
		int width = 0;
		int len = 0;
		u_long arg;
	    again:
		switch((c = *fmt++))
		{
		case '-':
		    flags |= PF_MINUS;
		    goto again;

		case '+':
		    flags |= PF_PLUS;
		    goto again;

		case ' ':
		    flags |= PF_SPC;
		    goto again;

		case '#':
		    flags |= PF_HASH;
		    goto again;

		case '0':
		    flags |= PF_ZERO;
		    goto again;

		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
		    while(IS_DIGIT(c))
		    {
			width = width * 10 + (c - '0');
			c = *fmt++;
		    }
		    fmt--;
		    goto again;

		case 'h':
		    flags |= PF_SHORT;
		    goto again;

		case 'l':
		    flags |= PF_LONG;
		    goto again;
		}

		if(flags & PF_LONG)
		    arg = va_arg(args, u_long);
		else if(flags & PF_SHORT)
		    arg = (u_long)va_arg(args, u_short);
		else
		    arg = (u_long)va_arg(args, u_int);

		switch(c)
		{
		    char tmpbuf[40];
		    char *tmp;
		    char *digits;
		    int radix;
		    bool is_signed;

		case 'n':
		    /* Store the number of characters output so far in *arg. */
		    *(int *)arg = buf - orig;
		    break;

		case 'i':
		case 'd':
		    is_signed = TRUE;
		    goto do_decimal;

		case 'u':
		case 'Z':
		    is_signed = FALSE;
		do_decimal:
		    digits = "0123456789";
		    radix = 10;
		    goto do_number;

		case 'o':
		    is_signed = TRUE;
		    digits = "01234567";
		    radix = 8;
		    if(flags & PF_HASH)
		    {
			*buf++ = '0';
			len++;
		    }
		    goto do_number;

		case 'b':
		    is_signed = FALSE;
		    digits = "01";
		    radix = 2;
		    if(flags & PF_HASH)
		    {
			buf = stpcpy(buf, "0b");
			len += 2;
		    }
		    goto do_number;

		case 'p':
		    if(arg == 0)
		    {
			/* NULL pointer */
			(char *)arg = "(nil)";
			goto do_string;
		    }
		    flags |= PF_HASH;
		    /* FALL THROUGH */

		case 'x':
		    digits = "0123456789abcdef";
		    if(flags & PF_HASH)
		    {
			buf = stpcpy(buf, "0x");
			len += 2;
		    }
		    goto do_hex;
		case 'X':
		    digits = "0123456789ABCDEF";
		    if(flags & PF_HASH)
		    {
			buf = stpcpy(buf, "0X");
			len += 2;
		    }
		do_hex:
		    is_signed = FALSE;
		    radix = 16;
		    /* FALL THROUGH */

		do_number:
		    if(is_signed)
		    {
			if((long)arg < 0)
			{
			    *buf++ = '-';
			    arg = (u_long)(0 - (long)arg);
			    len++;
			}
			else if(flags & PF_PLUS)
			{
			    *buf++ = '+';
			    len++;
			}
			else if(flags & PF_SPC)
			{
			    *buf++ = ' ';
			    len++;
			}
		    }
		    tmp = tmpbuf;
		    if(arg == 0)
		    {
			*tmp++ = digits[0];
			len++;
		    }
		    else
		    {
			while(arg > 0)
			{
			    *tmp++ = digits[arg % radix];
			    len++;
			    arg /= radix;
			}
		    }
		    if(width > len)
		    {
			if(flags & PF_MINUS)
			{
			    /* left justify. */
			    while(tmp != tmpbuf)
				*buf++ = *(--tmp);
			    memset(buf, ' ', width - len);
			    buf += width - len;
			}
			else
			{
			    memset(buf, (flags & PF_ZERO) ? '0' : ' ',
				   width - len);
			    buf += width - len;
			    while(tmp != tmpbuf)
				*buf++ = *(--tmp);
			}
		    }
		    else
		    {
			while(tmp != tmpbuf)
			    *buf++ = *(--tmp);
		    }
		    break;			

		case 'c':
		    if(width > 1)
		    {
			if(flags & PF_MINUS)
			{
			    *buf = (char)c;
			    memset(buf+1, ' ', width - 1);
			    buf += width; 
			}
			else
			{
			    memset(buf, (flags & PF_ZERO) ? '0' : ' ', width - 1);
			    *buf += width;
			    buf[-1] = (char)c;
			}
		    }
		    else
			*buf++ = (char)arg;
		    break;

		case 's':
		do_string:
		    if(width > 0)
		    {
			len = strlen((char *)arg);
			if(width <= len)
			    buf = stpcpy(buf, (char *)arg);
			else
			{
			    if(flags & PF_MINUS)
			    {
				buf = stpcpy(buf, (char *)arg);
				memset(buf, ' ', width - len);
				buf += width - len;
			    }
			    else
			    {
				memset(buf, (flags & PF_ZERO) ? '0' : ' ',
				       width - len);
				buf = stpcpy(buf + (width - len), (char *)arg);
			    }
			}
		    }
		    else
			buf = stpcpy(buf, (char *)arg);
		    break;
		}
	    }
	    else
		*buf++ = *fmt++;
	}
    }
    *buf++ = 0;
}

void
ksprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvsprintf(buf, fmt, args);
    va_end(args);
}

void
kvprintf(const char *fmt, va_list args)
{
    kvsprintf(printf_buf, fmt, args);
    print(printf_buf, strlen(printf_buf));
}

void
kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}
