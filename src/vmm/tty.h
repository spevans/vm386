/* tty.h -- Binds together a virtual keyboard and a virtual video device.
   John Harper. */

#ifndef _VMM_TTY_H
#define _VMM_TTY_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/video.h>
#include <vmm/kbd.h>
#include <vmm/vkbd.h>
#include <vmm/lists.h>
#include <stdarg.h>

enum tty_kbd_type {
    Raw, Cooked, Virtual
};

struct tty {
    list_node_t node;
    struct task *owner;
    enum tty_kbd_type kbd_type;
    union {
	struct kbd raw;
	struct cooked_kbd cooked;
	struct vkbd virtual;
    } kbd;
    struct kbd *base_kbd;
    struct video video;
    u_char current_page;
    short x, y;
    list_t rl_history;			/* For readline() */
    int rl_history_size;
};

#define TTY_COLS(tty)	((tty)->video.mi.cols)
#define TTY_ROWS(tty)	((tty)->video.mi.rows)
#define TTY_BUF(tty)	((tty)->video.mi.buffer)
#define TTY_PAGE_SIZE(tty) ((tty)->video.mi.page_size)
#define TTY_NUM_PAGES(tty) ((tty)->video.mi.pages)
#define TTY_CRT_I(tty)	((tty)->video.vga.crt_i)
#define TTY_CRT_D(tty)	((tty)->video.vga.crt_d)

/* Forward references. */
struct task;

struct tty_module {
    struct module base;

    struct tty *(*open_tty)(struct task *task, enum tty_kbd_type kbd,
			    const char *video_type, u_long video_flags);
    void (*close_tty)(struct tty *tty);

    void (*to_front)(struct tty *tty);
    void (*next_tty)(void);
    void (*prev_tty)(void);

    void (*print)(struct tty *tty, const char *buf);
    void (*printn)(struct tty *tty, const char *buf, size_t length);
    void (*printf)(struct tty *tty, const char *fmt, ...);
    void (*vprintf)(struct tty *tty, const char *fmt, va_list args);
    void (*clear)(struct tty *tty);
    void (*clear_chars)(struct tty *tty, size_t length);
    void (*set_cursor)(struct tty *tty, short x, short y);
    void (*scroll_up)(struct tty *tty);
    void (*read_cursor)(struct tty *tty);
    void (*set_page)(struct tty *tty, u_int pageno);

    char *(*readline)(struct tty *tty, size_t *lengthp);
    int (*get_char)(struct tty *tty);

    void (*beep)(struct tty *tty);
    void (*speaker_on)(struct tty *tty, u_int freq);
    void (*speaker_off)(struct tty *tty);

    struct tty *current_tty;
};

extern struct tty_module tty_module;

#ifdef TTY_MODULE
/* from readline.c */
extern void rl_free_history_list(list_t *list);
extern char *readline(struct tty *tty, size_t *lengthp);
#endif

#endif /* _VMM_TTY_H */
