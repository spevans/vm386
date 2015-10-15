/* video.c -- Generic video handling.
   John Harper. */

#include <vmm/video.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/tty.h>
#include <vmm/string.h>

#define kprintf kernel->printf

static bool video_expunge(void);
static void add_video(struct video_ops *new);
static bool init_video(struct video *v, struct task *task,
		       const char *type, u_long flags);
static inline u_char video_inb(struct video *v, u_short port);
static inline void video_outb(struct video *v, u_char byte, u_short port);
static char *video_find_page(struct video *v, u_int page);
static bool video_set_mode(struct video *v, u_int mode);
static u_long video_in(struct vm *vm, u_short port, int size);
static void video_out(struct vm *vm, u_short port, int size, u_long value);
static void kill_video(struct video *v);
static void switch_video(struct video *v);

struct video_module video_module =
{
    MODULE_INIT("video", SYS_VER, video_init, NULL, NULL, video_expunge),
    init_video, add_video, kill_video, switch_video, video_inb, video_outb,
    video_find_page, video_set_mode,
    vga_get_mode, vga_save_regs, vga_load_regs, vga_disable_video,
    vga_enable_video, vga_set_mode
};

struct io_handler video_mda_io = {
    NULL, "video-mda", 0x3b0, 0x3bb, video_in, video_out
};

struct io_handler video_io = {
    NULL, "video", 0x3c0, 0x3df, video_in, video_out
};

static struct vm_module *vm;

struct video *viewed_video;

struct video_ops *video_list;

bool
video_init(void)
{
    viewed_video = NULL;
    video_list = NULL;

    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm == NULL)
	return FALSE;

    mda_init();
    cga_init();

    /* Somehow this has to arrange that accesses of the following I/O ports
       are trapped and passed to the I/O handlers video_read_port() and
       video_write_port(). The ports are:

	3B0-3BB ;MDA
	3C0-3CF ;EGA/VGA
	3D0-3DF ;CGA

       and any other video-related ports (SVGA?) */
    vm->add_io_handler(NULL, &video_mda_io);
    vm->add_io_handler(NULL, &video_io);

    return TRUE;
}

static bool
video_expunge(void)
{
    if(video_module.base.open_count == 0)
    {
	struct video_ops *ops = video_list;
	vm->remove_io_handler(NULL, &video_io);
	vm->remove_io_handler(NULL, &video_mda_io);
	while(ops != NULL)
	{
	    if(ops->expunge != NULL)
		ops->expunge();
	    ops = ops->next;
	}
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

static void
add_video(struct video_ops *new)
{
    forbid();
    new->next = video_list;
    video_list = new;
    permit();
}

static bool
init_video(struct video *v, struct task *task, const char *type, u_long flags)
{
    struct video_ops *ops;
    forbid();
    ops = video_list;
    while(ops != NULL)
    {
	if(!strcasecmp(ops->name, type))
	{
	    permit();
	    v->ops = ops;
	    v->task = task;
	    v->in_view = FALSE;
	    if(ops->init)
		return ops->init(v, flags);
	    else
		return TRUE;		/* ?? */
	}
	ops = ops->next;
    }
    permit();
    return FALSE;
}

static inline u_char
video_inb(struct video *v, u_short port)
{
    DB(("video_inb: v=%p port=%#x\n", v, port));
    if(v->ops->inb)
	return v->ops->inb(v, port);
    else
	return 0;
}

static inline void
video_outb(struct video *v, u_char byte, u_short port)
{
    DB(("video_outb: v=%p byte=%#x port=%#x\n", v, byte, port));
    if(v->ops->outb)
	v->ops->outb(v, byte, port);
}

static char *
video_find_page(struct video *v, u_int page)
{
    if(v->ops->find_page)
	return v->ops->find_page(v, page);
    else
	return 0;
}

static bool
video_set_mode(struct video *v, u_int mode)
{
    if(v->ops->set_mode)
	return v->ops->set_mode(v, mode);
    else
	return FALSE;
}

static u_long
video_in(struct vm *vm, u_short port, int size)
{
    DB(("video_in: vm=%p port=%x size=%d\n", vm, port, size));
    if(size != 1)
	return 0;
    return (u_long)video_inb(&vm->tty->video, port);
}

static void
video_out(struct vm *vm, u_short port, int size, u_long value)
{
    DB(("video_out: vm=%p port=%x size=%d val=%x\n", vm, port, size, value));
    if(size == 1)
	video_outb(&vm->tty->video, (u_char)value, port);
}

static void
kill_video(struct video *v)
{
    DB(("kill_video: v=%p\n", v));
    if(viewed_video == v)
	switch_video(NULL);
    if(v->ops->kill)
	v->ops->kill(v);
}

static void
switch_video(struct video *v)
{
    DB(("switch_video: v=%p\n", v));
    forbid();
    if(viewed_video != v)
    {
	if(viewed_video != NULL)
	{
	    video_module.vga_disable_video(&viewed_video->vga);
	    if(viewed_video->ops->switch_from)
		viewed_video->ops->switch_from(viewed_video);
	    viewed_video->in_view = FALSE;
	}
	viewed_video = v;
	if(v != NULL)
	{
	    v->in_view = TRUE;
	    video_module.vga_load_regs(&v->vga);
	    if(v->ops->switch_to)
		v->ops->switch_to(v);
	    video_module.vga_enable_video(&v->vga);
	}
    }
    permit();
}
