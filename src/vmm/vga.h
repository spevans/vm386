/* vga.h -- Definitions for using the VGA.
   John Harper. */

#ifndef __VMM_VGA_H
#define __VMM_VGA_H

#include <vmm/types.h>

/* VGA port addresses. */
#define VGA_CRT_IC		0x3d4
#define VGA_CRT_DC		0x3d5
#define VGA_CRT_IM		0x3b4
#define VGA_CRT_DM		0x3b5
#define VGA_ATTR_IW		0x3c0
#define VGA_ATTR_R		0x3c1
#define VGA_GRAPH_I		0x3ce
#define VGA_GRAPH_D		0x3cf
#define VGA_SEQ_I		0x3c4
#define VGA_SEQ_D		0x3c5
#define VGA_PEL_IR		0x3c7
#define VGA_PEL_IW		0x3c8
#define VGA_PEL_D		0x3c9
#define VGA_MISC_R		0x3cc
#define VGA_MISC_W		0x3c2
#define VGA_IS1_RC		0x3da
#define VGA_IS1_RM		0x3ba

/* Number of registers in each port. */
#define VGA_CRT_COUNT		24
#define VGA_ATTR_COUNT		21
#define VGA_GRAPH_COUNT		9
#define VGA_SEQ_COUNT		5
#define VGA_MISC_COUNT		1

/* Indexes of each set of registers in a whole set. */
#define VGA_CRT_REGS		0
#define VGA_ATTR_REGS		(VGA_CRT_REGS+VGA_CRT_COUNT)
#define VGA_GRAPH_REGS		(VGA_ATTR_REGS+VGA_ATTR_COUNT)
#define VGA_SEQ_REGS		(VGA_GRAPH_REGS+VGA_GRAPH_COUNT)
#define VGA_MISC_REG		(VGA_SEQ_REGS+VGA_SEQ_COUNT)
#define VGA_TOTAL_REGS		(VGA_MISC_REG+VGA_MISC_COUNT)

struct vga_info {
    u_char regs[VGA_TOTAL_REGS];
    u_short crt_i, crt_d;
    u_short is1_r;
};


/* Prototypes. */

#ifdef VIDEO_MODULE

struct video;
struct mode_info;

extern bool vga_get_mode(u_int mode, const u_char **regsp,
			 const struct mode_info **infop);
extern void vga_save_regs(struct vga_info *inf);
extern void vga_load_regs(struct vga_info *inf);
extern void vga_disable_video(struct vga_info *inf);
extern void vga_enable_video(struct vga_info *inf);
extern bool vga_set_mode(struct video *v, u_int mode);

#endif /* VIDEO_MODULE */
#endif /* __VMM_VGA_H */
