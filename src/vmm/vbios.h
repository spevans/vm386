/* vbios.h -- Definitions for the virtual BIOS. */

#ifndef __VMM_VBIOS_H
#define __VMM_VBIOS_H

#ifndef __ASM__

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/page.h>
#include <vmm/vm.h>
#include <vmm/time.h>

struct vbios {
    struct vm *vm;
    page *code;
    struct vm_kill_handler kh;
    struct timer_req int15_timer;
};

struct vbios_module {
    struct vxd_module base;
    void (*delete)(struct vm *vm);
};

extern struct vbios_module vbios_module;

extern unsigned char vbios_code16[];
extern unsigned int vbios_code16_length;

#define __PACK__ __attribute__ ((packed))

struct bios_data {
    u_short	com1_base, com2_base, com3_base, com4_base;
    u_short	lpt1_base, lpt2_base, lpt3_base, lpt4_base;

    u_short	equipment_data;
    u_char	reserved1[1];

    u_short	low_memory __PACK__;
    u_char	reserved2[2];

    u_char	kbd_flags[2], alt_keypad;
    u_short	kbd_buf_head, kbd_buf_tail;
    u_short	kbd_buf[16];

    u_char	drive_recal, drive_motor, motor_off_ticks;
    u_char	drive_status, fdc_status[7];

    u_char	display_mode;
    u_short	display_cols, display_buf_size, display_buf_start;
    u_short	cursor_pos[8], cursor_type;
    u_char	active_page;
    u_short	crt_base __PACK__;
    u_char	crt_3x8_val, crt_3x9_val;

    u_char	reserved3[5];

    u_long	timer_counter;
    u_char	timer_overflow;

    u_char	break_key;
    u_short	reset_flag;

    u_char	fixed_disk_status, num_fixed_disks;
    u_char	reserved4[2];

    u_char	lpt1_time_out, lpt2_time_out, lpt3_time_out, lpt4_time_out;
    u_char	com1_time_out, com2_time_out, com3_time_out, com4_time_out;

    u_short	kbd_buf_start, kbd_buf_end;

    u_char	display_rows;
    u_short	char_height __PACK__;
    u_char	video_control_states[2], video_display_data, display_cc;

    u_char	drive_step_rate;
    u_char	fixed_disk_controller_stat, fixed_disk_controller_error;
    u_char	fixed_disk_int_ctrl, reserved5[1], drive_media_state[2];
    u_char	reserved6[2], current_cyl[2];

    u_char	kbd_data[2];

    u_char	reserved7[0x54];

    u_short	print_screen_stat;
};

#define BIOS_DATA ((struct bios_data *)0x400)

#ifdef VBIOS_MODULE

/* from video.c */
extern void vbios_video_handler(struct vm *vm, struct vm86_regs *regs);

/* from disk.c */
extern void vbios_disk_handler(struct vm *vm, struct vm86_regs *regs,
			       struct vbios *vbios);

/* from misc.c */
extern void unimplemented(char *name, struct vm86_regs *regs);
extern void vbios_ser_handler(struct vm *vm, struct vm86_regs *regs);
extern void vbios_par_handler(struct vm *vm, struct vm86_regs *regs);
extern void vbios_timer_handler(struct vm *vm, struct vm86_regs *regs);
extern void vbios_sys_handler(struct vm *vm, struct vm86_regs *regs,
			      struct vbios *vbios);

#endif /* VBIOS_MODULE */
#else /* __ASM__ */

/* An instruction of the form 0x63, X is trapped by the VM86 invl
   handler and passed to the 32-bit mode ARPL handler. */
#define ARPL(x)			\
	.byte	0x63, x

#endif /* __ASM__ */

#define VBIOS_SEG 0xff00

/* Indexes of fields in the BIOS data area. */

#define COM1_BASE		0x00
#define COM2_BASE		0x02
#define COM3_BASE		0x04
#define COM4_BASE		0x06
#define LPT1_BASE		0x08
#define LPT2_BASE		0x0a
#define LPT3_BASE		0x0c
#define LPT4_BASE		0x0e

#define EQUIPMENT_DATA		0x10

#define LOW_MEMORY		0x13

#define KBD_FLAGS		0x17
#define ALT_KEYPAD		0x19
#define KBD_BUF_HEAD		0x1a
#define KBD_BUF_TAIL		0x1c
#define KBD_BUF			0x1e

#define DRIVE_RECAL		0x3e
#define DRIVE_MOTOR		0x3f
#define MOTOR_OFF_TICKS		0x40
#define DRIVE_STATUS		0x41
#define FDC_STATUS		0x42

#define DISPLAY_MODE		0x49
#define DISPLAY_COLS		0x4a
#define DISPLAY_BUF_SIZE	0x4c
#define DISPLAY_BUF_START	0x4e
#define CURSOR_POS		0x50
#define CURSOR_TYPE		0x60
#define ACTIVE_PAGE		0x62
#define CRT_BASE		0x63
#define CRT_3x8_VAL		0x65
#define CRT_3x9_VAL		0x66

#define TIMER_COUNTER		0x6c
#define TIMER_OVERFLOW		0x70

#define BREAK_KEY		0x71
#define RESET_FLAG		0x72

#define FIXED_DISK_STATUS	0x74
#define NUM_FIXED_DISKS		0x75

#define LPT1_TIME_OUT		0x78
#define LPT2_TIME_OUT		0x79
#define LPT3_TIME_OUT		0x7a
#define LPT4_TIME_OUT		0x7b
#define COM1_TIME_OUT		0x7c
#define COM2_TIME_OUT		0x7d
#define COM3_TIME_OUT		0x7e
#define COM4_TIME_OUT		0x7f

#define KBD_BUF_START		0x80
#define KBD_BUF_END		0x82

#define DISPLAY_ROWS		0x84
#define CHAR_HEIGHT		0x85
#define VIDEO_CONTROL_STATES	0x87
#define VIDEO_DISPLAY_DATA	0x89
#define DISPLAY_CC		0x8a
#define DRIVE_STEP_RATE		0x8b
#define FIXED_DISK_CONTROLLER_STAT 0x8c
#define FIXED_DISK_CONTROLLER_ERROR 0x8d
#define FIXED_DISK_INT_CTRL	0x8e
#define DRIVE_MEDIA_STATE	0x90
#define CURRENT_CYL		0x94

#define KBD_DATA		0x96  

#define PRINT_SCREEN_STAT	0x100

#endif /* __VMM_VBIOS_H */
