/* kbd.c -- Keyboard driver.
   John Harper. */

#include <vmm/types.h>
#include <vmm/string.h>
#include <vmm/io.h>
#include <vmm/kbd.h>
#include <vmm/keycodes.h>
#include <vmm/kernel.h>
#include <vmm/irq.h>
#include <vmm/map.h>
#include <vmm/tty.h>
#include <vmm/tasks.h>

#ifdef TEST_KBD
# ifndef GO32
#  error Need go32
# endif
# include <test/go32prt.h>
#else
# include <vmm/shell.h>
# define kprintf kernel->printf
#endif

/* The console in focus. */
struct kbd *kbd_focus;


/* Initialise the keyboard structure pointed to by KBD. All function vectors
   are set to NULL. */
void
init_kbd(struct kbd *kbd)
{
    kbd->use_key = NULL;
    kbd->switch_from = NULL;
    kbd->switch_to = NULL;
    kbd->in_focus = FALSE;
}


/* Static data for the keyboard handler. */

/* This maps scan codes prefixed by E0 to their keycodes. */
static u_char e0_key_codes[128] =
{
    0, 0, 0, 0, 0, 0, 0, 0,					/* 0x00 */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,					/* 0x10 */
    0, 0, 0, 0, K_E0_KP_ENTER, K_E0_RIGHT_CTRL, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,					/* 0x20 */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, K_E0_KP_SLASH, 0, K_E0_PRT_SCR,		/* 0x30 */
    K_E0_RIGHT_ALT, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, K_E0_BREAK, K_E0_HOME,			/* 0x40 */
    K_E0_UP, K_E0_PAGE_UP, 0, K_E0_LEFT, 0, K_E0_RIGHT, 0, K_E0_END,
    K_E0_DOWN, K_E0_PAGE_DOWN, K_E0_INSERT, K_E0_DELETE, 0, 0, 0, 0, /* 0x50 */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,					/* 0x60 */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,					/* 0x70 */
    0, 0, 0, 0, 0, 0, 0, 0,
};

/* The current shift state. This is recorded to allow us to maintain
   a consistent shift state when switching consoles. */
static int shift_state;

static void
set_shift_state(int shift_mask, u_char up_code)
{
    if(up_code)
	shift_state &= ~shift_mask;
    else
	shift_state |= shift_mask;
}

/* This is the keyboard handler. It's not designed to be called straight
   from the IRQ; instead it should be called by the kernel message queue
   dispatcher, sometime after the IRQ1 occurred. */
void
kbd_handler(void)
{
    static int prev_scancode;
    kbd_send_command(KB_8042_DISABLE_KBD);
    kbd_wait();
    while((inb_p(0x64) & KB_STAT_OUTPUT_FULL) != 0)
    {
	u_char scan_code = inb_p(0x60);
	u_char key_code = 0;
	DB(("kh: scan_code=%#x\n", scan_code));
	if((scan_code == 0xe0) || (scan_code == 0xe1))
	    prev_scancode = scan_code;
	else
	{
	    u_char up_code = scan_code & 0x80;
	    key_code = scan_code & 0x7f;
	    if(prev_scancode == 0xe0)
	    {
		/* Ignore the e0 aa / e0 2a and e0 b6 / e0 36 code
		   sequences. We maintain our own shift/lock state. */
		if(key_code != 0x2a && key_code != 0x36)
		{
		    DB(("kh: E0 sequence (%02X %02X) ", prev_scancode, key_code));
		    key_code = e0_key_codes[key_code];
		    DB(("=> %02X\n", key_code));
		}
		else
		{
		    DB(("kh: Ignoring E0 sequence (%02X %02X)\n",
		        prev_scancode, key_code));
		    key_code = 0;
		}
		prev_scancode = 0;
	    }
	    else if(prev_scancode == 0xe1 && key_code == 0x1d)
	    {
		key_code = 0;
		prev_scancode = 0x100;
	    }
	    else if(prev_scancode == 0x100 && key_code == 0x45)
	    {
		key_code = K_E1_PAUSE;
		prev_scancode = 0;
	    }
	    /* Now key_code has the value of the key just pressed or zero
	       and up_code is 0x80 if it was released. */
	    if(key_code)
	    {
		DB(("kbd: key_code=%#02x, %s\n",
		    key_code, up_code ? "Up" : "Down"));
		if((key_code == K_E0_DELETE)
			&& (shift_state & S_ALT)
			&& (shift_state & S_CTRL))
		{
		    do_hard_reset();
		}
#ifndef TEST_KBD
		else if((((key_code == K_KP_PLUS) || (key_code == K_KP_MINUS)
			  || (key_code == K_ESC))
			 && (shift_state & S_ALT))
			|| (key_code == K_SYS_REQ))
		{
		    if(up_code == 0)
		    {
			if((key_code == K_ESC) || (key_code == K_SYS_REQ))
			{
			    struct shell_module *shell = (struct shell_module *)kernel->open_module("shell", SYS_VER);
			    if(shell != NULL)
			    {
				if(key_code == K_SYS_REQ)
				    shell->shell_to_front();
				else
				    shell->spawn_subshell();
				kernel->close_module((struct module *)shell);
			    }
			}
			else
			{
			    struct tty_module *tty = (struct tty_module *)kernel->open_module("tty", SYS_VER);
			    if(tty != NULL)
			    {
				if(key_code == K_KP_PLUS)
				    tty->next_tty();
				else if(key_code == K_KP_MINUS)
				    tty->prev_tty();
				kernel->close_module((struct module *)tty);
			    }
			}
		    }
		}
#endif
		else
		{
		    switch(key_code)
		    {
		    case K_LEFT_ALT:
			set_shift_state(S_LEFT_ALT, up_code);
			break;
			
		    case K_E0_RIGHT_ALT:
			set_shift_state(S_RIGHT_ALT, up_code);
			break;
			
		    case K_LEFT_SHIFT:
			set_shift_state(S_LEFT_SHIFT, up_code);
			break;
			
		    case K_RIGHT_SHIFT:
			set_shift_state(S_RIGHT_SHIFT, up_code);
			break;
			
		    case K_LEFT_CTRL:
			set_shift_state(S_LEFT_CTRL, up_code);
			break;
			
		    case K_E0_RIGHT_CTRL:
			set_shift_state(S_RIGHT_CTRL, up_code);
			break;
		    }
		    if(kbd_focus && kbd_focus->use_key)
		    {
			kbd_focus->use_key(kbd_focus, shift_state,
					   key_code, up_code);
		    }
		}
	    }
	}
    }
    /* I'm not sure if it's a good idea to have the keyboard disabled
       for such a long time. Maybe I should only disable it while actually
       messing with the kbd I/O ports... */
    kbd_send_command(KB_8042_ENABLE_KBD);
}


/* Keyboard utility functions. */

/* Send the byte DATA-BYTE to the keyboard data port (60H). This retries
   if necessary and returns TRUE if it eventually received an ACK, FALSE
   otherwise. */
bool
kbd_send_data(u_char data_byte)
{
    int retries = 3;
    do {
	u_char c;
	kbd_wait();
	outb_p(data_byte, 0x60);
	kbd_wait_output();
	kbd_send_command(KB_8042_DISABLE_KBD);
	c = inb_p(0x60);
	kbd_send_command(KB_8042_ENABLE_KBD);
	if(c == KB_REP_ACK)
	    return TRUE;
	if(c != KB_REP_RESEND)
	    return FALSE;
    } while(--retries >= 0);
    return FALSE;
}

/* Set the lock-status LEDs to LED-STATE. */
bool
kbd_set_leds(u_char led_state)
{
    kbd_send_data(KB_KBD_SET_LEDS);
    return kbd_send_data(led_state);
}

/* Switch the in-focus keyboard to NEW-KBD. */
void
kbd_switch(struct kbd *new_kbd)
{
    forbid();
    if(new_kbd != kbd_focus)
    {
	if(kbd_focus)
	{
	    if(kbd_focus->switch_from)
		kbd_focus->switch_from(kbd_focus, shift_state);
	    kbd_focus->in_focus = FALSE;
	}
	kbd_focus = new_kbd;
	if(new_kbd != NULL)
	{
	    new_kbd->in_focus = TRUE;
	    if(kbd_focus->switch_to)
		kbd_focus->switch_to(kbd_focus, shift_state);
	}
    }
    permit();
}

bool
init_kbd_irq(void)
{
    return kernel->alloc_queued_irq(1, kbd_handler, "keyboard");
}

void
do_hard_reset(void)
{
    kprintf("Resetting...\n");
    /* Ignore memory check on reboot. */
    *TO_LOGICAL(0x472, u_short *) = 0x1234;
    cli();
    while(1)
    {
	int i;
	for(i = 0; i < 100; i++)
	{
	    int j;
	    kbd_wait();
	    for(j = 0; j < 100000; j++) ;
	    outb(KB_8042_PULSE_OUTPUT_PORT | 0xe, 0x64);
	}
	kprintf("Huh? Can't reset..\n");
    }
}
