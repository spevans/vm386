/* video.c -- VBIOS INT 10 handling.
   John Harper. */

#include <vmm/vbios.h>
#include <vmm/vm.h>
#include <vmm/tasks.h>
#include <vmm/tty.h>
#include <vmm/video.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/segment.h>

#define kprintf kernel->printf

extern struct video_module *video;
extern struct tty_module *tty;

static inline void
bios_scroll_up_one(struct tty *vmtty, short page)
{
    char *buf = (TTY_BUF(vmtty) + (page * TTY_PAGE_SIZE(vmtty)));
    memcpy_user(buf, buf + TTY_COLS(vmtty) * 2, TTY_COLS(vmtty) * 2
		* (TTY_ROWS(vmtty)-1));
    memsetw_user(buf + TTY_COLS(vmtty) * 2 * (TTY_ROWS(vmtty)-1), 0x0720,
		 TTY_COLS(vmtty));
}

static void
bios_print(struct tty *vmtty, u_char *str, bool in_user, size_t length,
	   u_char type, u_char def_attr, short col, short row, short page)
{
#define UPDATE_POINT \
    point = (TTY_BUF(vmtty) + (page * TTY_PAGE_SIZE(vmtty)) \
	     + (row * TTY_COLS(vmtty) + col) * 2)

    u_char *point;
    DB(("bios_print: str=%p in_user=%d length=%u type=%x def_attr=%x col=%d row=%d page=%d\n",
	str, in_user, length, type, def_attr, col, row, page));
    if(page >= TTY_NUM_PAGES(vmtty))
	return;
    UPDATE_POINT;
    while(length-- > 0)
    {
	u_char c = in_user ? get_user_byte(str++) : *str++;
	switch(c)
	{
	case 0x08: /* BS */
	    if(--col < 0)
	    {
		if(--row < 0)
		{
		    row = 0;
		    col = 0;
		}
		else
		    col = TTY_COLS(vmtty) - 1;
	    }
	    UPDATE_POINT;
	    break;
	case 0x0d: /* CR */
	    col = 0;
	    UPDATE_POINT;
	    break;
	case 0x0a: /* LF */
	    if(++row >= TTY_ROWS(vmtty))
	    {
		row--;
		bios_scroll_up_one(vmtty, page);
	    }
	    col = 0;
	    UPDATE_POINT;
	    break;
	case 0x07: /* BEL */
	    break;
	default:
	    if(++col >= TTY_COLS(vmtty))
	    {
		col = 0;
		if(++row >= TTY_ROWS(vmtty))
		{
		    row--;
		    bios_scroll_up_one(vmtty, page);
		}
		UPDATE_POINT;
	    }
	    put_user_byte(c, point++);
	    put_user_byte((type & 2)
			   ? (in_user ? get_user_byte(str++) : *str++)
			   : def_attr,
			  point++);
	    break;
	}
    }
    if(type & 1)
    {
	if(vmtty->current_page == page)
	    tty->set_cursor(vmtty, col, row);
	if(page < 8)
	    put_user_short(col | (row << 8), &BIOS_DATA->cursor_pos[page]);
    }
}

#define TTY_VMEM \
    (TTY_BUF(vmtty) + (vmtty->current_page * TTY_PAGE_SIZE(vmtty)))

static void
bios_scroll_up(struct tty *vmtty, u_int num_lines, u_char attr, u_int left_col,
	       u_int left_row, u_int right_col, u_int right_row)
{
    u_int i;
    DB(("bios_scroll_up: num_lines=%d attr=%d left_row=%d\n"
	"                left_col=%d right_col=%d right_row=%d\n",
	num_lines, attr, left_row, left_col, right_col, right_row));
    if((left_col >= right_col) || (left_row >= right_row))
	return;
    if((num_lines > (right_row - left_row)) || (num_lines == 0))
    {
	for(i = left_row; i <= right_row; i++)
	{
	    memsetw_user(TTY_VMEM + (left_col * 2) + (i * TTY_COLS(vmtty) *2),
			 attr << 8, right_col - left_col + 1);
	}
    }
    else
    {
        char *src, *dest;
        int count, width;
 
        count = right_col - left_col + 1;
        count *= 2;
 
        width = 2 * TTY_COLS(vmtty);
 
        src = TTY_VMEM + (left_col * 2);
        src += (left_row + num_lines) * width;
        dest = TTY_VMEM + (left_col * 2);
        dest += left_row * width; 
 
	for(left_row += num_lines - 1; left_row != right_row; left_row++)
	{
            memcpy_user(dest, src, count);
            dest += width;
            src += width;
	}
 
	while(num_lines--)
	{
	    memsetw_user(dest, (attr << 8) | 0x20, right_col - left_col + 1);
            dest += width;
	}
    }
}

static void
bios_scroll_down(struct tty *vmtty, u_int num_lines, u_char attr,
		 u_int left_col, u_int left_row, u_int right_col,
		 u_int right_row)
{
    u_int i;
    DB(("bios_scroll_down: num_lines=%d attr=%d left_row=%d\n"
	"                  left_col=%d right_col=%d right_row=%d\n",
	num_lines, attr, left_row, left_col, right_col, right_row));
    if((left_col >= right_col) || (left_row >= right_row))
	return;
    if((num_lines > (right_row - left_row)) || (num_lines == 0))
    {
	for(i = left_row; i <= right_row; i++)
	{
	    memsetw_user(TTY_VMEM + (left_col * 2) + (i * TTY_COLS(vmtty) *2),
			 attr << 8, right_col - left_col + 1);
	}
    }
    else
    {
        char *src, *dest;
        int count, width;
 
        count = right_col - left_col + 1;
        count *= 2;
 
        width = 2 * TTY_COLS(vmtty);
 
        src = TTY_VMEM + (left_col * 2);
        src += (right_row - num_lines) * width;
        dest = TTY_VMEM + (left_col * 2);
        dest += right_row * width;
 
        for(right_row -= (num_lines - 1); right_row != left_row; right_row--)
	{
            memcpy_user(dest, src, count);
            dest -= width;
            src -= width;
	}
 
        while(num_lines--)
	{
            memsetw_user(dest, (attr << 8) | 0x20, right_col - left_col + 1);
            dest -= width;
	}
    }
}

static inline char *
bios_find_page(struct tty *vmtty, u_char pageno)
{
    return (TTY_BUF(vmtty) + TTY_PAGE_SIZE(vmtty) * pageno);
}

void
vbios_video_handler(struct vm *vm, struct vm86_regs *regs)
{
    struct video *v = &vm->tty->video;
    DB(("vbios_video: vm=%p cs:eip=%x:%x eax=%x ecx=%x edx=%x\n",
	vm, regs->cs, regs->eip, regs->eax, regs->ecx, regs->edx));
    switch(GET8H(regs->eax))
    {
    case 0:
	{
	    u_char mode = GET8(regs->eax);
	    bool clear;
	    clear = (mode & 0x80) == 0;
	    mode &= 0x7f;
	    video->set_mode(v, mode);
	    if(clear)
	    {
		memsetl_user(v->mi.buffer, 0, (v->mi.page_size
					       * v->mi.pages) / 4);
	    }
	    put_user_byte(v->mode, &BIOS_DATA->display_mode);
	    put_user_short(v->mi.cols, &BIOS_DATA->display_cols);
	    put_user_byte(v->mi.rows - 1, &BIOS_DATA->display_rows);
	    put_user_short(v->mi.page_size, &BIOS_DATA->display_buf_size);
	    put_user_short(0, &BIOS_DATA->display_buf_start);
	    memsetw_user(&BIOS_DATA->cursor_pos, 0, 8);
	    put_user_byte(0, &BIOS_DATA->active_page);
	    put_user_short(v->vga.crt_i, &BIOS_DATA->crt_base);
	    break;
	}

    case 1:
	{
	    u_char top = GET8H(regs->ecx);
	    u_char bottom = GET8(regs->ecx);
	    video->outb(v, 10, TTY_CRT_I(vm->tty));
	    video->outb(v, top, TTY_CRT_D(vm->tty));
	    video->outb(v, 11, TTY_CRT_I(vm->tty));
	    video->outb(v, bottom, TTY_CRT_D(vm->tty));
	    put_user_short((top << 8) | bottom, &BIOS_DATA->cursor_type);
	    break;
	}

    case 2:
	{
	    u_char this_page = GET8H(regs->ebx);
	    put_user_short(GET16(regs->edx),
			   &BIOS_DATA->cursor_pos[this_page]);
	    if(this_page == vm->tty->current_page)
		tty->set_cursor(vm->tty, GET8(regs->edx), GET8H(regs->edx));
	    break;
	}

    case 3:
	regs->ecx = SET16(regs->ecx, get_user_short(&BIOS_DATA->cursor_type));
	regs->edx = SET16(regs->edx, get_user_short(&BIOS_DATA->cursor_pos[get_user_byte(&BIOS_DATA->active_page)]));
	break;

    case 4:
	regs->eax = SET8H(regs->eax, 0);
	break;

    case 5:
	tty->set_page(vm->tty, GET8(regs->eax));
	put_user_short(TTY_PAGE_SIZE(vm->tty) * vm->tty->current_page,
		       &BIOS_DATA->display_buf_start);
	put_user_byte(vm->tty->current_page, &BIOS_DATA->active_page);
	if(vm->tty->current_page < 8)
	{
	    u_short offset = get_user_short(&BIOS_DATA->cursor_pos[vm->tty->current_page]);
	    tty->set_cursor(vm->tty, offset & 255, offset >> 8);
	}
	break;

    case 6:
	bios_scroll_up(vm->tty, GET8(regs->eax), GET8H(regs->ebx),
		       GET8(regs->ecx), GET8H(regs->ecx),
		       GET8(regs->edx), GET8H(regs->edx));
	break;

    case 7:
	bios_scroll_down(vm->tty, GET8(regs->eax), GET8H(regs->ebx),
			 GET8(regs->ecx), GET8H(regs->ecx),
			 GET8(regs->edx), GET8H(regs->edx));
	break;

    case 8:
	{
	    u_char page = GET8H(regs->ebx);
	    u_short cursor = get_user_short(&BIOS_DATA->cursor_pos[page]);
	    char *buf = bios_find_page(vm->tty, page)
		+ ((cursor >> 8) * TTY_COLS(vm->tty) + (cursor & 255)) * 2;
	    regs->eax = SET16(regs->eax, get_user_short((u_short *)buf));
	    break;
	}

    case 9:
	{
	    u_char page = GET8H(regs->ebx);
	    u_short cursor = get_user_short(&BIOS_DATA->cursor_pos[page]);
	    char *buf = bios_find_page(vm->tty, page);
	    memset_user(buf + ((cursor >> 8) * TTY_COLS(vm->tty)
			       + (cursor & 255)) * 2,
			GET8(regs->eax) | (GET8(regs->ebx) << 8),
			GET16(regs->ecx));
	    break;
	}

    case 10:
	{
	    u_char page = GET8H(regs->ebx);
	    u_short cursor = get_user_short(&BIOS_DATA->cursor_pos[page]);
	    char *buf = bios_find_page(vm->tty, page)
		+ (((cursor >> 8) * TTY_COLS(vm->tty) + (cursor & 255)) * 2);
	    int count = GET16(regs->ecx);
	    u_char c = GET8(regs->eax);
	    while(count > 0)
	    {
		put_user_byte(c, buf);
		buf += 2;
		count--;
	    }
	    break;
	}

    case 14:
	{
	    u_char c = GET8(regs->eax);
	    bios_print(vm->tty, &c, FALSE, 1, 1, 0x07, vm->tty->x,
		       vm->tty->y, GET8H(regs->ebx));
	    break;
	}

    case 15:
	regs->eax = SET16(regs->eax, get_user_byte(&BIOS_DATA->display_mode)
			  | (TTY_COLS(vm->tty) << 8));
	regs->ebx = SET8H(regs->ebx, 0);
	break;

    case 19:
	bios_print(vm->tty, (u_char *)((regs->es << 4) + GET16(regs->ebp)),
		   TRUE, GET16(regs->ecx), GET8(regs->eax), GET8(regs->ebx),
		   GET8(regs->edx), GET8H(regs->edx), GET8H(regs->ebx));
	break;

    case 26:
	if(GET8(regs->eax) == 0)
	{
	    regs->eax = SET8(regs->eax, 26);
	    regs->ebx = SET16(regs->ebx, !strcmp(vm->tty->video.ops->name,
						 "mda") ? 1 : 2);
	}
	break;

    default:
        unimplemented("vbios int 10", regs);
    }
}
