/* cga.c -- Virtual CGA
   John Harper. */

#include <vmm/video.h>
#include <vmm/crt6845.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <vmm/tasks.h>

#define kprintf kernel->printf

static bool init_cga(struct video *v, u_long flags);
static u_char cga_inb(struct video *v, u_short port);
static void cga_outb(struct video *v, u_char byte, u_short port);
static void cga_switch_to(struct video *new);
static void cga_switch_from(struct video *old);
static void cga_kill(struct video *v);
static char *cga_find_page(struct video *v, u_int page);
static bool cga_set_mode(struct video *v, u_int mode);
static void cga_expunge(void);

static struct video_ops cga_ops =
{
    0, "cga",
    init_cga, cga_inb, cga_outb, cga_switch_to, cga_switch_from,
    cga_kill, cga_find_page, cga_set_mode, cga_expunge
};

static const u_char default_mc6845_regs[18] =
{
    0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C,
    0x02, 0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00,
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

#define CGA_DEF_MODE (CGA_MODE_80 | CGA_MODE_COLOR | CGA_MODE_VIDEO_ON)
#define CGA_DEF_COLOR 0
#define CGA_DEF_STAT (CGA_STAT_ACCESS_OK | CGA_STAT_VBLANK)


bool
cga_init(void)
{
    video_module.add_video(&cga_ops);
    return TRUE;
}

void
cga_expunge(void)
{
}


/* I/O port virtualisation. Basically this is just a write-through cache. */

static void
set_mode_select(struct video *v, u_char byte)
{
    if(byte != v->data.cga.mode_select)
    {
	u_int mode;
	if(byte & CGA_MODE_320_APA)
	    mode = 4;
	else if(byte & CGA_MODE_640_APA)
	    mode = 6;
	else if(byte & CGA_MODE_80)
	    mode = 2;
	else
	    mode = 4;
	if((mode != 6) && ((byte & CGA_MODE_COLOR) == 0))
	    mode |= 1;
	video_module.vga_set_mode(v, mode);
	v->data.cga.mode_select = byte;
    }
}

static void
set_color_select(struct video *v, u_char byte)
{
    if(byte != v->data.cga.color_select)
    {
	v->vga.regs[VGA_ATTR_REGS+0x11] = byte & 15;
	if(byte & 0x20)
	{
	    /* Cyan, Magenta & White. */
	    v->vga.regs[VGA_ATTR_REGS+0] = 0x00;
	    v->vga.regs[VGA_ATTR_REGS+1] = 0x03;
	    v->vga.regs[VGA_ATTR_REGS+2] = 0x05;
	    v->vga.regs[VGA_ATTR_REGS+3] = 0x07;
	}
	else
	{
	    /* Green, Red & Yellow. */
	    v->vga.regs[VGA_ATTR_REGS+0] = 0x00;
	    v->vga.regs[VGA_ATTR_REGS+1] = 0x02;
	    v->vga.regs[VGA_ATTR_REGS+2] = 0x04;
	    v->vga.regs[VGA_ATTR_REGS+3] = 0x0e;
	}
	/* Need to do background intensity bit as well. */
	v->data.cga.color_select = byte;
	if(v->in_view)
	{
	    forbid();
	    video_module.vga_load_regs(&v->vga);
	    permit();
	}
    }
}

static u_char
cga_inb(struct video *v, u_short port)
{
    DB(("cga_inb: v=%p port=%#x\n", v, port));
    switch(port)
    {
    case 0x3D0: case 0x3D2:
    case 0x3D4: case 0x3D6:
	return v->data.cga.index_register;

    case 0x3D1: case 0x3D3:
    case 0x3D5: case 0x3D7:
	if(v->data.cga.index_register <= 15)
	    return v->data.cga.mc6845_registers[v->data.cga.index_register];
	else
	    return 0;

    case 0x3D8:
	return v->data.cga.mode_select;

    case 0x3D9:
	return v->data.cga.color_select;

    case 0x3DA:
	/* Since many programs test this register to see when it's safe to
	   write to the video buffer, but this isn't really necessary
	   on EGA (?), twiddle some bits in this reg each time.. */
	v->data.cga.status_reg ^= (CGA_STAT_ACCESS_OK | CGA_STAT_VBLANK);
	return v->data.cga.status_reg;

    default:
	return (u_char)-1;
    }
}

static void
cga_outb(struct video *v, u_char byte, u_short port)
{
    DB(("cga_outb: v=%p byte=%#x port=%#x\n", v, byte, port));
    switch(port)
    {
    case 0x3D0: case 0x3D2:
    case 0x3D4: case 0x3D6:
	v->data.cga.index_register = byte;
	break;

    case 0x3D1: case 0x3D3:
    case 0x3D5: case 0x3D7:
	v->data.cga.data_register = byte;
	if(v->data.cga.index_register <= 15)
	{
	    char reg;
	    v->data.cga.mc6845_registers[v->data.cga.index_register] = byte;
	    reg = mc6845_to_vga_regs[v->data.cga.index_register];
	    if(reg != -1)
	    {
		if(v->in_view)
		{
		    outb(reg, v->vga.crt_i);
		    outb(byte, v->vga.crt_d);
		}
		v->vga.regs[VGA_CRT_REGS+reg] = byte;
	    }
	}
	break;

    case 0x3D8:
	DB(("cga_outb: mode_select=%x\n", byte));
	set_mode_select(v, byte);
	break;

    case 0x3D9:
	DB(("cga_outb: color_select=%x\n", byte));
	set_color_select(v, byte);
	break;
    }
}



static inline void
map_cga_buffer(struct video *v, bool phys_buf)
{
    size_t i;
    DB(("map_cga_buffer: v=%p phys_buf=%#d\n", v, phys_buf));
    forbid();
    for(i = 0; i < 4; i++)
    {
	kernel->set_pte(v->task->page_dir, CGA_VIDEO_MEM + (i * PAGE_SIZE),
			(phys_buf ? (CGA_VIDEO_MEM + (i * PAGE_SIZE))
			 : TO_PHYSICAL(v->data.cga.video_buffer[i])) | PTE_USER
			| PTE_READ_WRITE | PTE_PRESENT);
    }
    permit();
}

/* Initialises V to virtualise a CGA device for the task TASK. */
bool
init_cga(struct video *v, __attribute__ ((unused)) u_long flags)
{
    int i;
    for(i = 0; i < 4; i++)
    {
	v->data.cga.video_buffer[i] = kernel->alloc_page();
	memsetw(v->data.cga.video_buffer[i], 0x720, PAGE_SIZE/2);
    }
    v->data.cga.mode_select = CGA_DEF_MODE;
    v->data.cga.color_select = CGA_DEF_COLOR;
    v->data.cga.status_reg = CGA_DEF_STAT;
    memcpy(v->data.cga.mc6845_registers, default_mc6845_regs, 18);
    video_module.vga_set_mode(v, 3);
    map_cga_buffer(v, FALSE);
    return TRUE;
}

static void
cga_kill(struct video *v)
{
    int i;
    for(i = 0; i < 4; i++)
	kernel->free_page(v->data.cga.video_buffer[i]);
}

static void
cga_switch_from(struct video *old)
{
    int i;
    DB(("cga_switch_from: old=%p\n", old));
    for(i = 0; i < 4; i++)
    {
	memcpy(old->data.cga.video_buffer[i],
	       TO_LOGICAL(CGA_VIDEO_MEM + (i * PAGE_SIZE), void *), PAGE_SIZE);
    }
    map_cga_buffer(old, FALSE);
}

static void
cga_switch_to(struct video *new)
{
    int i;
    DB(("cga_switch_to: new=%p\n", new));
    for(i = 0; i < 4; i++)
    {
	memcpy(TO_LOGICAL(CGA_VIDEO_MEM + (i * PAGE_SIZE), void *),
	       new->data.cga.video_buffer[i], PAGE_SIZE);
    }
    map_cga_buffer(new, TRUE);
}

static char *
cga_find_page(struct video *v, u_int page)
{
    if(page < 4)
    {
	if(v->in_view)
	    return TO_LOGICAL(CGA_VIDEO_MEM + page*4096, char *);
	else
	    return (char *)v->data.cga.video_buffer[page];
    }
    else
	return 0;
}

static bool
cga_set_mode(struct video *v, u_int mode)
{
    if(mode >= 7)
	return FALSE;
    return video_module.vga_set_mode(v, mode);
}
