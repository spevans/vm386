/* mda.h -- Definitions for the virtual MDA driver.

   This file should only be included by <vmm/video.h>

   John Harper. */

#ifndef _VMM_MDA_H
#define _VMM_MDA_H

#include <vmm/page.h>

struct mda_data {
    page *video_buffer;
    u_char control_port;
    u_char status_port;
    u_char index_register;
    u_char data_register;
    u_char mc6845_registers[18];
};

#define MDA_VIDEO_MEM 0x000B0000
#define EGA_VIDEO_MEM 0x000B8000

/* Control port bits. */
#define MDA_CTRL_ENABLE_MDA	1
#define MDA_CTRL_ENABLE_VIDEO	8
#define MDA_CTRL_ENABLE_BLINK	32

/* Flags to init_mda() */
#define INIT_MDA_KEEP		1


/* Function prototypes. */

#ifdef VIDEO_MODULE

extern bool mda_init(void);

#endif /* VIDEO_MODULE */
#endif /* _VMM_MDA_H */
