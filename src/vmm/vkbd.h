/* vkbd.h -- Virtual keyboard definitions.
   John Harper. */

#ifndef __VMM_VKBD_H
#define __VMM_VKBD_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/kbd.h>

struct vkbd {
    struct kbd kbd;
    struct vm *vm;

    /* 8042 state. */
    u_char status_byte;
    u_char command_byte;
    u_char input_port;
    u_char output_port;
    u_char this_8042_command;

    /* Keyboard controller state. */

    /* High word contains flags, low word has the scan code and ASCII code
       of the key; low-byte = scan_code, High = ascii (or 0). */
    u_long buf[16];
    int in_pos, out_pos;
    u_char this_kbd_command;
    u_char lock_state;
    u_char led_state;

    /* To emulate the 8255 functions. */
    u_char output_8255;

    int shift_state;
};

/* Flags for the vkbd buffer. */
#define VK_BUF_UP_KEY	0x00010000
#define VK_BUF_SHIFT	0x00020000

extern inline bool
vkbd_is_empty(struct vkbd *vk)
{
    return vk->in_pos == vk->out_pos;
}



struct vkbd_module {
    struct module base;
    void (*init_vkbd)(struct vkbd *vk, bool display_is_mda);
};

extern struct vkbd_module vkbd_module;

#endif /* __VMM_VKBD_H */
