/* lib.c -- Common fs related functions.
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <stdarg.h>

#ifndef TEST
# define kprintf kernel->printf
# define kvsprintf kernel->vsprintf
#else
# define kprintf printf
# define kvsprintf vsprintf
#include <stdio.h>
#endif

/* Really need buffered I/O for this type of thing :-(  */

inline int
fs_putc(u_char c, struct file *fh)
{
    return write_file(&c, 1, fh);
}

inline int
fs_getc(struct file *fh)
{
    u_char c;
    int val = read_file(&c, 1, fh);
    return val != 1 ? ((val == 0) ? EOF : val) : (int)c;
}

char *
fs_read_line(char *buf, size_t bufsiz, struct file *fh)
{
    char *ptr = buf;
    while(--bufsiz != 0)
    {
	int c = fs_getc(fh);
	if(c < 0)
	    break;
	*ptr++ = c;
	if(c == '\n')
	    break;
    }
    *ptr = 0;
    return (ptr == buf) ? NULL : buf;
}

inline int
fs_write_string(const char *str, struct file *fh)
{
    return write_file(str, strlen(str), fh);
}

int
fs_fvprintf(struct file *fh, const char *fmt, va_list args)
{
    char buf[512];
    kvsprintf(buf, fmt, args);
    return fs_write_string(buf, fh);
}

int
fs_fprintf(struct file *fh, const char *fmt, ...)
{
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = fs_fvprintf(fh, fmt, args);
    va_end(args);
    return ret;
}
