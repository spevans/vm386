/* cga.h -- Definitions for the virtual CGA driver.

   This file should only be included by <vmm/video.h>

   John Harper. */

#ifndef _VMM_CGA_H
#define _VMM_CGA_H

#include <vmm/page.h>

struct cga_data {
    page *video_buffer[4];
    u_char mode_select;
    u_char color_select;
    u_char status_reg;
    u_char index_register;
    u_char data_register;
    u_char mc6845_registers[18];
};

#define CGA_VIDEO_MEM 0xb8000

/* Mode select bits. */
#define CGA_MODE_80		0x01
#define CGA_MODE_320_APA	0x02
#define CGA_MODE_COLOR		0x04
#define CGA_MODE_VIDEO_ON	0x08
#define CGA_MODE_640_APA	0x10
#define CGA_MODE_BLINKING	0x20

/* Status bits. */
#define CGA_STAT_ACCESS_OK	0x01
#define CGA_STAT_LIGHT_PEN_TRIG	0x02
#define CGA_STAT_LIGHT_PEN_OFF	0x04
#define CGA_STAT_VBLANK		0x08


/* Prototypes. */

#ifdef VIDEO_MODULE

extern bool cga_init(void);

#endif /* VIDEO_MODULE */
#endif /* __VMM_CGA_H */
