/* tty.c -- TTY module. Binds togther the kbd and video drivers.
   John Harper. 
 
   tty_beep() added by Simon Evans */

#include <vmm/tty.h>
#include <vmm/kernel.h>
#include <vmm/string.h>
#include <vmm/tasks.h>
#include <vmm/shell.h>
#include <vmm/io.h>
#include <vmm/time.h>
#include <stdarg.h>

#define kprintf kernel->printf

struct kernel_module *kernel;
struct video_module *video;
struct kbd_module *kbd;
struct vkbd_module *vkbd;

static list_t tty_list;

static void tty_beep(struct tty *tty);


/* Output. */

/* Scroll the contents of TTY's display up one line. */
static void
tty_scroll_up(struct tty *tty)
{
    /* This should (hopefully) scroll the contents of the screen buffer
       up one row then blank the exposed bottom row. */
    char *buf = video->find_page(&tty->video, tty->current_page);
    memcpy(buf, buf + TTY_COLS(tty) * 2, TTY_COLS(tty) * 2
	   * (TTY_ROWS(tty)-1));
    memsetw(buf + TTY_COLS(tty) * 2 * (TTY_ROWS(tty)-1),
	    0x0720, TTY_COLS(tty));
}

/* Set the cursor of TTY to position (X,Y) */
static void
tty_set_cursor(struct tty *tty, short x, short y)
{
    u_short offset;
    if((x > TTY_COLS(tty)) || (y > TTY_ROWS(tty)))
	return;
    tty->x = x; tty->y = y;
    offset = x + (y * TTY_COLS(tty));
    video->outb(&tty->video, 14, TTY_CRT_I(tty));
    video->outb(&tty->video, offset >> 8, TTY_CRT_D(tty));
    video->outb(&tty->video, 15, TTY_CRT_I(tty));
    video->outb(&tty->video, offset & 255, TTY_CRT_D(tty));
}

/* Update the cached cursor position in TTY. */
static void
tty_read_cursor(struct tty *tty)
{
    u_short offset;
    video->outb(&tty->video, 14, TTY_CRT_I(tty));
    offset = video->inb(&tty->video, TTY_CRT_D(tty)) << 8;
    video->outb(&tty->video, 15, TTY_CRT_I(tty));
    offset |= video->inb(&tty->video, TTY_CRT_D(tty));
    tty->x = offset % TTY_COLS(tty);
    tty->y = offset / TTY_COLS(tty);
}

/* Clear the display of TTY and set the cursor to the top-left corner. */
static void
tty_clear(struct tty *tty)
{
    char *buf = video->find_page(&tty->video, tty->current_page);
    memsetw(buf, 0x0720, TTY_COLS(tty) * TTY_ROWS(tty));
    tty_set_cursor(tty, 0, 0);
}

/* Clear LENGTH characters in the display of TTY from the current cursor
   position. This will wrap when it gets to the end of a line. */
static void
tty_clear_chars(struct tty *tty, size_t length)
{
    char *buf = video->find_page(&tty->video, tty->current_page);
    long offset = (tty->y * TTY_COLS(tty) * 2) + (tty->x * 2);
    if(offset + length >= TTY_PAGE_SIZE(tty))
	length = TTY_PAGE_SIZE(tty) - offset;
    memsetw(buf + offset, 0x0720, length);
}

/* Print LENGTH characters from the string TEXT at the current cursor
   position in TTY. NL, TAB and BEL characters are treated specially. The
   cursor is advanced over the displayed text. */
static void
tty_printn(struct tty *tty, const u_char *text, size_t length)
{
    char *buf = video->find_page(&tty->video, tty->current_page);
    u_char *cursor_char;
    u_char c;

#define UPDATE_CURS \
    cursor_char = (buf + (tty->y * TTY_COLS(tty) * 2) + (tty->x * 2))

    UPDATE_CURS;
    while(length-- > 0)
    {
	c = *text++;
	if((c == '\n') || (tty->x >= TTY_COLS(tty)))
	{
	    tty->x = 0;
	    if(++tty->y == TTY_ROWS(tty))
	    {
		tty_scroll_up(tty);
		tty->y--;
	    }
	    UPDATE_CURS;
	    if(c == '\n')
		continue;
	}
	else if(c == '\t')
	{
	    int new_x = (tty->x + 8) & ~7;
	    memsetw(cursor_char, 0x0720, new_x - tty->x);
	    tty->x = new_x;
	    UPDATE_CURS;
	    continue;
	}
	else if(c == 0x07) 
        {
            tty_beep(tty);
	    continue;
	}
	cursor_char[0] = c;
	cursor_char[1] = 7;
	cursor_char += 2;
	tty->x++;
    }
    tty_set_cursor(tty, tty->x, tty->y);
}

/* Use tty_printn() to print the zero-terminated string BUF in TTY. */
static inline void
tty_print(struct tty *tty, const u_char *buf)
{
    tty_printn(tty, buf, strlen(buf));
}

/* Do a vprintf() into the tty TTY. */
static void
tty_vprintf(struct tty *tty, const u_char *fmt, va_list args)
{
    u_char buf[1024];
    kernel->vsprintf(buf, fmt, args);
    tty_print(tty, buf);
}

/* Do a printf() into the tty TTY. */
static void
tty_printf(struct tty *tty, const u_char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tty_vprintf(tty, fmt, args);
    va_end(args);
}

/* Set the character under the cursor in TTY to C. */
static inline void
set_char(struct tty *tty, char c)
{
    char *buf = video->find_page(&tty->video, tty->current_page);
    buf[(tty->y * TTY_COLS(tty) * 2) + (tty->x * 2)] = c;
    buf[(tty->y * TTY_COLS(tty) * 2) + (tty->x * 2) + 1] = 7;
}

/* Set the current display page of TTY to PAGENO. */
static void
tty_set_page(struct tty *tty, u_int pageno)
{
    DB(("tty_set_page: tty=%p pageno=%d\n", tty, pageno));
    if(pageno < TTY_NUM_PAGES(tty))
    {
	u_long bufstart = pageno * (TTY_PAGE_SIZE(tty) / 2);
	video->outb(&tty->video, 12, TTY_CRT_I(tty));
	video->outb(&tty->video, bufstart >> 8, TTY_CRT_D(tty));
	video->outb(&tty->video, 13, TTY_CRT_I(tty));
	video->outb(&tty->video, bufstart & 255, TTY_CRT_D(tty));
    }
}


/* Input. */

/* Read one character from TTY. If an error occurs return -1. */
static int
tty_get_char(struct tty *tty)
{
    if(tty->kbd_type == Cooked)
	return kbd->cooked_read_char(&tty->kbd.cooked);
    return -1;
}


/* Beeping. */

/* Turn the speaker on in TTY at a frequency of FREQ hertz. */
static inline void
speaker_on(struct tty *tty, u_int freq)
{
    u_short divisor = 1193180 / freq;
    outb(0xb6, 0x43);
    outb(divisor & 0xFF, 0x42);
    outb((divisor >> 8 & 0xFF), 0x42);
    outb((inb(0x61) | 0x03), 0x61);
}

/* Turn off the speaker in TTY. */
static inline void
speaker_off(struct tty *tty)
{
    outb((inb(0x61) & 0xFC), 0x61);
}

/* Make a beep of frequency FREQ hertz in TTY for LENGTH*18.2 seconds. */
static inline void
mksound(struct tty *tty, unsigned int freq, unsigned int length)
{
    speaker_on(tty, freq);
    kernel->sleep_for_ticks(length * TICKS_18_TO_1024);
    speaker_off(tty);
}

/* Beep in the tty TTY. */
static void
tty_beep(struct tty *tty)
{
    mksound(tty, 600, 2);
}


/* TTY switching. */

/* Send tty TTY to the foreground. */
static void
tty_to_front(struct tty *tty)
{
    if(tty == tty_module.current_tty)
	return;
    forbid();
    remove_node(&tty->node);
    prepend_node(&tty_list, &tty->node);
    tty_module.current_tty = tty;
    permit();
    kbd->switch_focus(tty->base_kbd);
    video->switch_video(&tty->video);
}

/* Cycle through the list of tty's forwards. */
static void
next_tty(void)
{
    forbid();
    if(!list_empty_p(&tty_list))
    {
	struct tty *tty = (struct tty *)tty_list.head->succ;
	if(tty->node.succ != NULL)
	{
	    if(tty_module.current_tty != NULL)
	    {
		remove_node(&tty_module.current_tty->node);
		append_node(&tty_list, &tty_module.current_tty->node);
	    }
	    tty_to_front(tty);
	}
    }
    permit();
}

/* Cycle through the list of tty's backwards. */
static void
prev_tty(void)
{
    forbid();
    if(!list_empty_p(&tty_list))
    {
	struct tty *tty = (struct tty *)tty_list.tailpred;
	tty_to_front(tty);
    }
    permit();
}


/* TTY structure handling. */

/* Open a new tty for the task TASK. Logical keyboard is of type
   TTY-KBD-TYPE, video is of type VIDEO-TYPE,VIDEO-FLAGS. Returns
   a null pointer if an error occurs. */
static struct tty *
open_tty(struct task *task, enum tty_kbd_type kbd_type,
	 const char *video_type, u_long video_flags)
{
    struct tty *new = kernel->calloc(sizeof(struct tty), 1);
    if(new != NULL)
    {
	new->owner = task;
	new->kbd_type = kbd_type;
	switch(kbd_type)
	{
	case Raw:
	    kbd->init_kbd(&new->kbd.raw);
	    new->base_kbd = &new->kbd.raw;
	    break;

	case Cooked:
	    kbd->init_cooked(&new->kbd.cooked);
	    kbd->cooked_set_task(&new->kbd.cooked);
	    new->base_kbd = &new->kbd.cooked.kbd;
	    break;

	case Virtual:
	    if((vkbd == NULL)
	       && !(vkbd = (struct vkbd_module *)kernel->open_module("vkbd", SYS_VER)))
	    {
		goto error;
	    }
	    vkbd->init_vkbd(&new->kbd.virtual, !strcasecmp(video_type, "mda"));
	    if(task->flags & TASK_VM)
		new->kbd.virtual.vm = task->user_data;
	    else
		new->kbd.virtual.vm = NULL;
	    new->base_kbd = &new->kbd.virtual.kbd;
	    break;

	default:
	    goto error;
	}
	if(!video->init_video(&new->video, task, video_type, video_flags))
	{
	error:
	    kernel->free(new);
	    return NULL;
	}
	new->current_page = 0;
	tty_read_cursor(new);
	init_list(&new->rl_history);
	new->rl_history_size = 0;
	forbid();
	append_node(&tty_list, &new->node);
	tty_to_front(new);
	permit();
    }
    return new;
}

/* Close the tty TTY. */
static void
close_tty(struct tty *tty)
{
    forbid();
    remove_node(&tty->node);
    if(tty_module.current_tty == tty)
    {
	if(list_empty_p(&tty_list))
	    tty_module.current_tty = NULL;
	else
	    tty_to_front((struct tty *)tty_list.head);
    }
    permit();
    video->kill_video(&tty->video);
    rl_free_history_list(&tty->rl_history);
    kernel->free(tty);
}


/* Some shell commands. */

#define DOC_ttyinfo "ttyinfo\n\
List the opened ttys."
int
cmd_ttyinfo(struct shell *sh, int argc, char **argv)
{
    struct tty *x;
    forbid();
    x = (struct tty *)tty_list.head;
    sh->shell->printf(sh, "%-8s %-8s %-8s\n", "Addr.", "Kbd", "Video");
    while(x->node.succ != NULL)
    {
	char *kbd_type;
	switch(x->kbd_type)
	{
	case Raw:
	    kbd_type = "Raw";
	    break;
	case Cooked:
	    kbd_type = "Cooked";
	    break;
	case Virtual:
	    kbd_type = "Virtual";
	    break;
	default:
	    kbd_type = "Unknown";
	}
	sh->shell->printf(sh, "%-8x %-8s %-8s %dx%d (%d,%d)\n",
			  x, kbd_type, x->video.ops->name,
			  TTY_COLS(x), TTY_ROWS(x),
			  x->x, x->y);
	x = (struct tty *)x->node.succ;
    }
    permit();
    return 0;
}

static void
db_print(const char *str, size_t len)
{
    tty_printn(tty_module.current_tty, str, len);
}

#define DOC_dbprint "dbprint\n\
Make kprintf() print all output to the *current* tty."
int
cmd_dbprint(struct shell *sh, int argc, char **argv)
{
    kernel->set_print_func(db_print);
    return 0;
}

struct shell_cmds tty_cmds =
{
    0, { CMD(ttyinfo), CMD(dbprint), END_CMD }
};



static bool
tty_init(void)
{
    init_list(&tty_list);
    kbd = (struct kbd_module *)kernel->open_module("kbd", SYS_VER);
    if(kbd != NULL)
    {
	video = (struct video_module *)kernel->open_module("video", SYS_VER);
	if(video != NULL)
	{
	    kernel->add_shell_cmds(&tty_cmds);
	    return TRUE;
	}
	kernel->close_module((struct module *)kbd);
    }
    return FALSE;
}

static bool
tty_expunge(void)
{
    if((tty_module.base.open_count == 0)
       && list_empty_p(&tty_list))
    {
	if(vkbd != NULL)
	    kernel->close_module((struct module *)vkbd);
	kernel->remove_shell_cmds(&tty_cmds);
	return TRUE;
    }
    return FALSE;
}

struct tty_module tty_module = 
{
    MODULE_INIT("tty", SYS_VER, tty_init, NULL, NULL, tty_expunge),
    open_tty, close_tty,
    tty_to_front, next_tty, prev_tty,
    tty_print, tty_printn, tty_printf, tty_vprintf, tty_clear,
    tty_clear_chars, tty_set_cursor, tty_scroll_up, tty_read_cursor,
    tty_set_page,
    readline, tty_get_char,
    tty_beep, speaker_on, speaker_off,
};
