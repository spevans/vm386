/* video.h -- Definitions for the video module.
   John Harper. */

#ifndef _VMM_VIDEO_H
#define _VMM_VIDEO_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/tasks.h>
#include <vmm/vga.h>

struct mode_info;
struct video;
struct video_ops;

#include <vmm/mda.h>
#include <vmm/cga.h>

struct mode_info {
    short cols, rows;
    char *buffer;			/* In USER space. */
    size_t page_size;
    u_char pages;
    bool text_mode, colour;
};

#define MAX_VIDEO_DATA 128

struct video {
    struct video_ops *ops;
    struct task *task;
    struct vga_info vga;
    struct mode_info mi;
    u_char mode;
    bool in_view;
    union {
	struct mda_data mda;
	struct cga_data cga;
	char padding[MAX_VIDEO_DATA];
    } data;
};

/* Gets a pointer to the data block in the video structure V, casting it to
   the type T. */
#define VIDEO_DATA(v,t) ((t)(&(v)->data))

struct video_ops {
    struct video_ops *next;
    const char *name;
    bool (*init)(struct video *v, u_long flags);
    u_char (*inb)(struct video *v, u_short port);
    void (*outb)(struct video *v, u_char byte, u_short port);
    void (*switch_to)(struct video *v);
    void (*switch_from)(struct video *v);
    void (*kill)(struct video *v);
    char *(*find_page)(struct video *v, u_int page);
    bool (*set_mode)(struct video *v, u_int mode);
    void (*expunge)(void);
};

struct video_module {
    struct module base;

    bool (*init_video)(struct video *v, struct task *task, const char *type,
		       u_long flags);
    void (*add_video)(struct video_ops *new);

    void (*kill_video)(struct video *v);
    void (*switch_video)(struct video *new);
    u_char (*inb)(struct video *v, u_short port);
    void (*outb)(struct video *v, u_char byte, u_short port);
    char *(*find_page)(struct video *v, u_int page);
    bool (*set_mode)(struct video *v, u_int mode);

    bool (*vga_get_mode)(u_int mode, const u_char **regsp,
			 const struct mode_info **infop);
    void (*vga_save_regs)(struct vga_info *inf);
    void (*vga_load_regs)(struct vga_info *inf);
    void (*vga_disable_video)(struct vga_info *inf);
    void (*vga_enable_video)(struct vga_info *inf);
    bool (*vga_set_mode)(struct video *v, u_int mode);
};


/* Function prototypes. */

#ifdef VIDEO_MODULE

extern bool video_init(void);
extern struct video_module video_module;

#endif /* VIDEO_MODULE */
#endif /* _VMM_VIDEO_H */
