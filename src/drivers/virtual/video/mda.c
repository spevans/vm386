/* mda.c -- Provides an MDA virtualisation.

   I'm not too sure about this. I'm trying to leave the video chips
   in their normal mode and to emulate the MDA, i.e. all I/O addresses
   are translated. But it doesn't inspire my confidence..

   John Harper. */

#include <vmm/video.h>
#include <vmm/crt6845.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <vmm/tasks.h>

#define kprintf kernel->printf

static bool init_mda(struct video *v, u_long flags);
static u_char mda_inb(struct video *v, u_short port);
static void mda_outb(struct video *v, u_char byte, u_short port);
static void mda_switch_to(struct video *new);
static void mda_switch_from(struct video *old);
static void mda_kill(struct video *v);
static char *mda_find_page(struct video *v, u_int page);
static bool mda_set_mode(struct video *v, u_int mode);
static void mda_expunge(void);

static struct video_ops mda_ops =
{
    0, "mda",
    init_mda, mda_inb, mda_outb, mda_switch_to, mda_switch_from,
    mda_kill, mda_find_page, mda_set_mode, mda_expunge
};

static const u_char default_mc6845_regs[18] =
{
    0x61, 0x50, 0x52, 0x0f, 0x19, 0x06, 0x19, 0x19,
    0x02, 0x0d, 0x0b, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

/* This translates MC6845 register indexes to their VGA/EGA CRT
   controller equivalents. A value of -1 means their is no equivalent. */
static const char mc6845_to_vga_regs[18] =
{
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15,
    -1, -1
};

#define DEFAULT_MDA_CONTROL (MDA_CTRL_ENABLE_MDA | MDA_CTRL_ENABLE_VIDEO \
			     | MDA_CTRL_ENABLE_BLINK)


bool
mda_init(void)
{
    video_module.add_video(&mda_ops);
    return TRUE;
}

void
mda_expunge(void)
{
}


/* I/O port virtualisation. Basically this is just a write-through cache. */

static inline void
set_control_port(struct video *v, u_char ctl)
{
    /* Hmm. */
}

static u_char
mda_inb(struct video *v, u_short port)
{
    DB(("mda_inb: v=%p port=%#x\n", v, port));
    switch(port)
    {
    case 0x3B0: case 0x3B2:
    case 0x3B4: case 0x3B6:
	return v->data.mda.index_register;

    case 0x3B1: case 0x3B3:
    case 0x3B5: case 0x3B7:
	if(v->data.mda.index_register <= 15)
	    return v->data.mda.mc6845_registers[v->data.mda.index_register];
	else
	    return 0;

    case 0x3B8:
	return v->data.mda.control_port;

    case 0x3BA:
	return 0;

    default:
	return (u_char)-1;
    }
}

static void
mda_outb(struct video *v, u_char byte, u_short port)
{
    DB(("mda_outb: v=%p byte=%#x port=%#x\n", v, byte, port));
    switch(port)
    {
    case 0x3B0: case 0x3B2:
    case 0x3B4: case 0x3B6:
	v->data.mda.index_register = byte;
	break;

    case 0x3B1: case 0x3B3:
    case 0x3B5: case 0x3B7:
	v->data.mda.data_register = byte;
	if(v->data.mda.index_register <= 15)
	{
	    char reg;
	    v->data.mda.mc6845_registers[v->data.mda.index_register] = byte;
	    reg = mc6845_to_vga_regs[v->data.mda.index_register];
	    if(reg != -1)
	    {
		if(v->in_view)
		{
		    DB(("mda_outb: setting reg %d to %#x\n", reg, byte));
		    outb(reg, v->vga.crt_i);
		    outb(byte, v->vga.crt_d);
		}
		v->vga.regs[VGA_CRT_REGS+reg] = byte;
	    }
	}
	break;

    case 0x3B8:
	v->data.mda.control_port = byte;
	set_control_port(v, byte);
	break;
    }
}



static inline void
map_mda_buffer(struct video *v, u_long phys_buffer)
{
    DB(("map_mda_buffer: v=%p phys_buffer=%#x\n", v, phys_buffer));
    forbid();
    kernel->set_pte(v->task->page_dir, MDA_VIDEO_MEM,
		    phys_buffer | PTE_USER | PTE_READ_WRITE | PTE_PRESENT);
    permit();
}

/* Initialises V to virtualise an MDA device for the task TASK. If KEEP
   is TRUE then screen contents and cursor position are inherited from
   the physical adaptor. */
bool
init_mda(struct video *v, u_long flags)
{
    v->data.mda.video_buffer = kernel->alloc_page();
    if((flags & INIT_MDA_KEEP) == 0)
	memsetw(v->data.mda.video_buffer, 0x0720, PAGE_SIZE/2);
    else
	memcpy(v->data.mda.video_buffer, TO_LOGICAL(EGA_VIDEO_MEM, char *),
	       PAGE_SIZE);
    v->data.mda.control_port = DEFAULT_MDA_CONTROL;
    v->data.mda.status_port = 0;			/* ? */
    memcpy(v->data.mda.mc6845_registers, default_mc6845_regs, 18);
    video_module.vga_set_mode(v, 7);
    if(flags & INIT_MDA_KEEP)
    {
	/* Get cursor. */
	u_char byte;
	outb(14, 0x3d4);
	byte = inb(0x3d5);
	v->vga.regs[VGA_CRT_REGS+14] = byte;
	v->data.mda.mc6845_registers[14] = byte;
	outb(15, 0x3d4);
	byte = inb(0x3d5);
	v->vga.regs[VGA_CRT_REGS+15] = byte;
	v->data.mda.mc6845_registers[15] = byte;
    }
    map_mda_buffer(v, TO_PHYSICAL(v->data.mda.video_buffer));
    return TRUE;
}

static void
mda_kill(struct video *v)
{
    kernel->free_page(v->data.mda.video_buffer);
}

static void
mda_switch_from(struct video *old)
{
    DB(("mda_switch_from: old=%p\n", old));
    map_mda_buffer(old, TO_PHYSICAL(old->data.mda.video_buffer));
    memcpy(old->data.mda.video_buffer,
	   TO_LOGICAL(MDA_VIDEO_MEM, void *), PAGE_SIZE);
}

static void
mda_switch_to(struct video *new)
{
    DB(("mda_switch_to: new=%p\n", new));
    memcpy(TO_LOGICAL(MDA_VIDEO_MEM, void *),
	   new->data.mda.video_buffer, PAGE_SIZE);
    map_mda_buffer(new, MDA_VIDEO_MEM);
}

static char *
mda_find_page(struct video *v, u_int page)
{
    if(page == 0)
    {
	return v->in_view ? TO_LOGICAL(MDA_VIDEO_MEM, char *)
	       : (char *)v->data.mda.video_buffer;
    }
    else
	return 0;
}

static bool
mda_set_mode(struct video *v, u_int mode)
{
    if(mode != 7)
	return FALSE;
    /* We must be in mode #7 already :) */
    return TRUE;
}
