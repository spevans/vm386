/* kbd.h -- Definitions for the keyboard device driver.
   John Harper. */

#ifndef __VMM_KBD_H
#define __VMM_KBD_H

#include <vmm/types.h>
#include <vmm/io.h>
#include <vmm/module.h>


/* Hardware stuff. */

/* Status register at port 64H (read) */
#define KB_STAT_OUTPUT_FULL	0x01
#define KB_STAT_INPUT_FULL	0x02
#define KB_STAT_SYS_FLAG	0x04
#define KB_STAT_COMMAND		0x08
#define KB_STAT_INHIBIT_SWITCH	0x10
#define KB_STAT_TX_ERROR	0x20
#define KB_STAT_TIME_OUT	0x40
#define KB_STAT_PARITY_ERROR	0x80

/* 8042 command byte. */
#define KB_CMD_INTERRUPT	0x01
#define KB_CMD_PS2_AUX_DEV_INT	0x02
#define KB_CMD_SYS_FLAG		0x04
#define KB_CMD_AT_INIHIBIT_OVERRIDE 0x08
#define KB_CMD_KBD_DISABLED	0x10
#define KB_CMD_AT_PC_MODE	0x20
#define KB_CMD_PS2_AUX_DEV_ENABLED 0x20
#define KB_CMD_PC_COMPAT	0x40

/* 8042 input port. */
#define KB_INP_2ND_256K		0x10
#define KB_INP_JUMP_INST	0x20
#define KB_INP_DISP_MDA		0x40
#define KB_INP_INHIBIT		0x80

/* 8042 output port. */
#define KB_OUTP_RESET_LINE	0x01
#define KB_OUTP_GATE_A20	0x02
#define KB_OUTP_OUTPUT_FULL	0x10
#define KB_OUTP_INPUT_EMPTY	0x20
#define KB_OUTP_KBD_CLK		0x40
#define KB_OUTP_KBD_DATA	0x80

/* 8042 commands. */
#define KB_8042_READ_CMD_BYTE	0x20
#define KB_8042_WRITE_CMD_BYTE	0x60
#define KB_8042_PASSWD_INST	0xA4
#define KB_8042_LOAD_PASSWD	0xA5
#define KB_8042_ENABLE_PASSWD	0xA6
#define KB_8042_SELF_TEST	0xAA
#define KB_8042_INTERFACE_TEST	0xAB
#define KB_8042_DUMP		0xAC
#define KB_8042_DISABLE_KBD	0xAD
#define KB_8042_ENABLE_KBD	0xAE
#define KB_8042_READ_INPUT_PORT	0xC0
#define KB_8042_READ_OUTPUT_PORT 0xD0
#define KB_8042_WRITE_OUTPUT_PORT 0xD1
#define KB_8042_READ_TEST_INPUTS 0xE0
#define KB_8042_PULSE_OUTPUT_PORT 0xF0		/* actually 0xF0 -> 0xFF */

/* Keyboard controller commands. */
#define KB_KBD_SET_LEDS		0xED
#define KB_KBD_ECHO		0xEE
#define KB_KBD_SEND_ID		0xF2
#define KB_KBD_SET_TYPEMATIC	0xF3
#define KB_KBD_ENABLE		0xF4
#define KB_KBD_RESET_STOP	0xF5
#define KB_KBD_SET_DEFAULT	0xF6
#define KB_KBD_RESEND		0xFE
#define KB_KBD_RESET		0xFF

/* Bits for the KB_KBD_SET_LEDS command. */
#define KB_LED_SCROLL_LOCK	0x01
#define KB_LED_NUM_LOCK		0x02
#define KB_LED_CAPS_LOCK	0x04

/* Replies from the keyboard to the system. */
#define KB_REP_OVERRUN		0x00
#define KB_REP_BAT_OK		0xAA
#define KB_REP_ECHO		0xEE
#define KB_REP_BREAK_PREFIX	0xF0
#define KB_REP_ACK		0xFA
#define KB_REP_BAT_FAIL		0xFC
#define KB_REP_RESEND		0xFE
#define KB_REP_KEY_ERROR	0xFF

/* 8255 Output port bits. */
#define KB_8255_SPEAKER_TIMER	0x01
#define KB_8255_SPEAKER_ON	0x02
#define KB_8255_KBD_ENABLE	0x40
#define KB_8255_KBD_ACK		0x80

/* Busy-wait for the input buffer to be empty. */
static inline void
kbd_wait(void)
{
    int i;
    for(i=0; i < 0x10000; i++)
    {
	if ((inb_p(0x64) & KB_STAT_INPUT_FULL) == 0)
	    break;
    }
}

/* Busy-wait for the output buffer to be full. */
static inline void
kbd_wait_output(void)
{
    int i;
    for(i=0; i < 0x10000; i++)
    {
	if ((inb_p(0x64) & KB_STAT_OUTPUT_FULL) != 0)
	    break;
    }
}

/* Send a command to the 8042. */
static inline void
kbd_send_command(u_char command_byte)
{
    kbd_wait();
    outb_p(command_byte, 0x64);
}



struct kbd {
    /* This function is called each time a key code is typed and KBD
       is in focus. */
    void (*use_key)(struct kbd *kbd, int shift_state, u_char key_code,
		    u_char up_code);

    /* Called when the keyboard focus is taken from KBD. */
    void (*switch_from)(struct kbd *kbd, int shift_state);

    /* Called when KBD is given the keyboard focus. Normally it has to set
       the keyboard leds to reflect it's current lock state. */
    void (*switch_to)(struct kbd *kbd, int shift_state);

    bool in_focus;
};

/* Bit definitions for coding the shift state. */
#define S_LEFT_SHIFT		0x01
#define S_RIGHT_SHIFT		0x02
#define S_SHIFT			(S_LEFT_SHIFT | S_RIGHT_SHIFT)
#define S_LEFT_CTRL		0x04
#define S_RIGHT_CTRL		0x08
#define S_CTRL			(S_LEFT_CTRL | S_RIGHT_CTRL)
#define S_LEFT_ALT		0x10
#define S_RIGHT_ALT		0x20
#define S_ALT			(S_LEFT_ALT | S_RIGHT_ALT)

/* And for the lock state. These are the same as in the keyboard controller. */
#define L_CAPS_LOCK		KB_LED_CAPS_LOCK
#define L_NUM_LOCK		KB_LED_NUM_LOCK
#define L_SCROLL_LOCK		KB_LED_SCROLL_LOCK


/* Cooked keyboard stuff. */

enum cooked_type {
    ct_None, ct_Func,
    ct_Task
};

#define COOKED_BUFSIZ 16

struct cooked_kbd {
    struct kbd kbd;
    int lock_state;
    enum cooked_type type;
    union {
	void (*func)(u_char c);
	struct {
	    struct task *task;
	    u_char buf[COOKED_BUFSIZ];
	    int in, out;
	} task;
    } receiver;
};

/* Keymap bits */
#define KM_SHIFT    1
#define KM_ALT      2
#define KM_CTRL     4
#define KM_NUM_LOCK 8


/* Keyboard module. */

struct kbd_module {
    struct module base;
    void (*init_kbd)(struct kbd *kbd);
    void (*switch_focus)(struct kbd *new);
    void (*init_cooked)(struct cooked_kbd *ck);
    void (*cooked_set_function)(struct cooked_kbd *ck, void (*func)(u_char));
    void (*cooked_set_task)(struct cooked_kbd *ck);
    u_char (*cooked_read_char)(struct cooked_kbd *ck);
    char *(*cook_keycode)(u_char key_code, int shift_state, int lock_state);
    bool (*set_leds)(u_char led_state);
};


/* Function prototypes. */

#ifdef KBD_MODULE

/* From kbd.c */
extern struct kbd *kbd_focus;
extern void init_kbd(struct kbd *kbd);
extern void kbd_handler(void);
extern bool kbd_send_data(u_char data_byte);
extern bool kbd_set_leds(u_char led_state);
extern void kbd_switch(struct kbd *new_kbd);
extern bool init_kbd_irq(void);
extern void do_hard_reset(void);

/* from cooked.c */
extern void init_cooked_kbd(struct cooked_kbd *ck);
extern void cooked_set_function(struct cooked_kbd *ck, void (*func)(u_char));
extern void cooked_set_task(struct cooked_kbd *ck);
extern u_char cooked_read_char(struct cooked_kbd *ck);
extern char *cook_keycode(u_char key_code, int shift_state, int lock_state);

/* from kbd_mod.c */
extern struct kbd_module kbd_module;

#endif /* KBD_MODULE */
#endif /* __VMM_KBD_H */
