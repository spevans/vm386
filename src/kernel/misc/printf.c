/* printf.c -- simple printf for the kernel
   John Harper. */

#ifdef TEST_PRINTF
# define TEST
#endif
#ifdef TEST
# include <sys/types.h>
# include <stdio.h>
# define PRINT_STRING(s) fputs(s, stdout)
# define __NO_TYPE_CLASHES
#endif

#include <vmm/types.h>
#include <vmm/string.h>
#include <vmm/shell.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/time.h>
#include <vmm/syslogd.h>

#ifndef TEST
# include <vmm/tasks.h>
#endif

static char printf_buf[1024];

/* static buffer to hold messages until syslogd is loaded */
#define KMSG_BUF_SIZE   4096
static char kern_msgs[KMSG_BUF_SIZE];
static int kern_msgs_idx = 0;
struct syslogd_module *syslogd;

static void (*kprint_func)(const char *, size_t);

#define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

/* Flags */
#define PF_MINUS 1
#define PF_PLUS  2
#define PF_SPC   4
#define PF_HASH  8
#define PF_ZERO  16
#define PF_SHORT 32
#define PF_LONG  64

/* Syntax of format specifiers in FMT is 
		`% [FLAGS] [WIDTH] [.PRECISION] [TYPE] CONV'
   (without the spaces). FLAGS can be any of:

	`-' Left justify the contents of the field.
	`+' Put a plus character in front of positive signed integers.
	` ' Put a space in from of positive signed integers (only if no `+').
	`#' Put `0' before octal numbers and `0x' or `0X' before hex ones.
	`0' Pad right-justified fields with zeros, not spaces.
	`*' Use the next argument as the field width.

   WIDTH is a decimal integer defining the field width (for justification
   purposes). Right justification is the default, use the `-' flag to
   change this.

   TYPE is an optional type conversion for integer arguments, it can be
   either `h' to specify a `short int' or `l' to specify a `long int'.

   CONV defines how to treat the argument, it can be one of:

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
		int precision = 0;
		int len = 0, num_len;
		u_long arg;
	    again:
		switch((c = *fmt++))
		{
		case '-':
		    flags |= PF_MINUS;
		    goto again;

		case '*':
		    /* dynamic field width. */
		    width = va_arg(args, int);
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

		case '.':
		    if((c = *fmt++) == '*')
			precision = va_arg(args, int);
		    else
		    {
			while(IS_DIGIT(c))
			{
			    precision = precision * 10 + (c - '0');
			    c = *fmt++;
			}
			fmt--;
		    }
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
		    num_len = len;
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
		    num_len = len - num_len;
		    if(precision != 0 && num_len < precision)
		    {
			while(num_len--)
			{
			    *tmp++ = digits[0];
			    len++;
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
		    if((char *)arg == NULL)
			arg = (u_long)"(nil)";
		    len = strlen((char *)arg);
		    if(precision > 0 && len > precision)
			len = precision;
		    if(width > 0)
		    {
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
		    {
			memcpy(buf, (char *)arg, len);
			buf += len;
			*buf = 0;
		    }
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

static void hack_print(const char *, size_t);

void
kvprintf(const char *fmt, va_list args)
{
    u_long len;
    kvsprintf(printf_buf, fmt, args);
    len = strlen(printf_buf);

#ifdef PRINT_STRING
    PRINT_STRING(printf_buf);
#elif 1
    if(kprint_func != NULL)
	kprint_func(printf_buf, len);
    else
	hack_print(printf_buf, len);
#if 1
    add_to_syslog(printf_buf, len);
#endif
#else
    hack_print(printf_buf, len);
#endif
}

void
kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

void
set_kprint_func(void (*func)(const char *, size_t))
{
    kprint_func = func;
}

void 
add_to_syslog(char *entry, int slen)
{
    char timebuf[26];
    static char log_entry[1024];
    struct time_bits tm;
    int len;
    static int idx = 0;

    strcpy(log_entry + idx, entry);
    idx += slen;
    if(entry[slen-1] != '\n') return;

    expand_time(current_time(), &tm);
    ksprintf(timebuf, "%3s %-2d %2d:%02d:%02d: kernel: ", tm.month_abbrev, tm.day,
        tm.hour, tm.minute, tm.second);
    len = idx + 25;
    if((kern_msgs_idx + len +1) < KMSG_BUF_SIZE){
        strcpy(kern_msgs + kern_msgs_idx, timebuf);
        strcpy(kern_msgs + kern_msgs_idx + 25, log_entry);
        kern_msgs_idx += len;
    }
    idx = 0;
    if(syslogd != NULL) {
        syslogd->syslog_cooked_entry(0, kern_msgs);
        kern_msgs_idx = 0;
        return;
    }
}

#ifndef TEST
/* Print LENGTH characters from the string TEXT. This is only used until
   the shell has been loaded. */
static void
hack_print(const char *text, size_t length)
{
#define SCR_BASE TO_LOGICAL(0xb8000, char *)
    static int cursor_x, cursor_y;
    char *cursor_char = (SCR_BASE + (cursor_y * 80 * 2) + (cursor_x * 2));
    char c;
    while(length-- > 0)
    {
	c = *text++;
	if((c == '\n') || (cursor_x >= 80))
	{
	    cursor_x = 0;
	    if(++cursor_y >= 25)
	    {
		memcpy(SCR_BASE, SCR_BASE + 160, 24 * 160);
		memsetw(SCR_BASE + (24 * 160), 0x0720, 160);
		cursor_y--;
	    }
	    cursor_char = (SCR_BASE + (cursor_y * 80 * 2)
			   + (cursor_x * 2));
	    if(c == '\n')
		continue;
	}
	else if(c == '\t')
	{
	    int new_x = (cursor_x + 8) & ~7;
	    memsetw(cursor_char, 0x0720, new_x - cursor_x);
	    cursor_x = new_x;
	    cursor_char = (SCR_BASE + (cursor_y * 80 * 2)
			   + (cursor_x * 2));
	    continue;
	}
	*cursor_char++ = c;
	*cursor_char++ = 0x07;
	cursor_x++;
    }
    outb(14, 0x3d4); outb((cursor_y * 80 + cursor_x) >> 8, 0x3d5);
    outb(15, 0x3d4); outb((cursor_y * 80 + cursor_x) & 0xff, 0x3d5);
}
#endif

#ifdef TEST_PRINTF
int
main(int ac, char **av)
{
    kprintf("%s, %l, %l, %Z, 0%o, 0x%x, %p, %p, %c %b.\n",
	    "foobar", 100, -100, -100, 30, 0xdeadbeef, av, NULL, 'X', 57);
    kprintf("%6s%-6s %+08d\n",
	    "foo", "bar", 123);
    return 0;
}
#endif

/*
 * Local Variables:
 * compile-command:"gcc -g -O2 -I.. -DTEST_PRINTF printf.c -o printf"
 * End:
 */
