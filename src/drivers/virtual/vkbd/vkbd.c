/* vkbd.c -- Keyboard virtualisation.
   John Harper. */

#include <vmm/types.h>
#include <vmm/io.h>
#include <vmm/vkbd.h>
#include <vmm/keycodes.h>
#include <vmm/vm.h>
#include <vmm/vpic.h>
#include <vmm/vpit.h>
#include <vmm/tty.h>
#include <vmm/vbios.h>
#include <vmm/kernel.h>

#define kprintf kernel->printf

static u_long vkbd_read_port(struct vm *vm, u_short port, int size);
static void vkbd_write_port(struct vm *vm, u_short port, int size, u_long val);
static void vkbd_use_key(struct kbd *kbd, int shift_state, u_char key_code,
			 u_char up_code);
static void vkbd_switch_from(struct kbd *kbd, int shift_state);
static void vkbd_switch_to(struct kbd *kbd, int shift_state);

struct kernel_module *kernel;
struct vm_module *vm;
struct kbd_module *kbd;
struct tty_module *tty;
struct vpic_module *vpic;
struct vpit_module *vpit;


/* Initialise the virtual keyboard structure pointed to by VK. */
void
init_vkbd(struct vkbd *vk, bool display_is_mda)
{
    kbd->init_kbd(&vk->kbd);
    vk->kbd.use_key = vkbd_use_key;
    vk->kbd.switch_from = vkbd_switch_from;
    vk->kbd.switch_to = vkbd_switch_to;
    vk->status_byte = KB_STAT_SYS_FLAG;
    vk->command_byte = KB_CMD_INTERRUPT | KB_CMD_SYS_FLAG | KB_CMD_PC_COMPAT;
    /* What else? */
    vk->input_port = KB_INP_2ND_256K | (display_is_mda ? KB_INP_DISP_MDA : 0);
    vk->output_port = KB_OUTP_INPUT_EMPTY; /* Flag A20? */
    vk->this_8042_command = 0;
    vk->in_pos = vk->out_pos = 0;
    vk->lock_state = 0;
    vk->led_state = 0;
    vk->this_kbd_command = 0;
    vk->output_8255 = KB_8255_KBD_ENABLE;
}



static inline void
make_vkbd_irq(struct vkbd *vk)
{
    if((vk->command_byte & KB_CMD_INTERRUPT))
#if 0
       && !(vk->command_byte & KB_CMD_KBD_DISABLED))
#endif
    {
	vpic->simulate_irq(vk->vm, 1);
    }
}

/* Add the code BYTE to the end of the input buffer in the virtual
   keyboard VK. If this buffer is currently empty and interrupts are
   enabled in VK, an IRQ1 will be simulated in its task. */
static void
vkbd_in(struct vkbd *vk, u_long val)
{
    int new_pos = (vk->in_pos + 1) % 16;
    DB(("vkbd_in: vk=%p val=%04x\n", vk, val));
    if(new_pos == vk->out_pos)
    {
	/* Buffer overflow. */
    }
    else
    {
	vk->buf[vk->in_pos] = val;
	vk->in_pos = new_pos;
	vk->status_byte |= KB_STAT_OUTPUT_FULL;
	vk->output_port |= KB_OUTP_OUTPUT_FULL;
	make_vkbd_irq(vk);
    }
}

/* Returns the next code in the input buffer of VK. */
static u_long
vkbd_out(struct vkbd *vk)
{
    u_long c = vk->buf[vk->out_pos];
    DB(("vkbd_out: vk=%p val=%04x\n", vk, c));
    vk->out_pos = (vk->out_pos + 1) % 16;
    if(vkbd_is_empty(vk))
    {
	vk->status_byte &= ~KB_STAT_OUTPUT_FULL;
	vk->output_port &= ~KB_OUTP_OUTPUT_FULL;
    }
    else
    {
	/* Hmm.. Some keyboard interrupt handlers don't keep
	   reading until the buffer is dry, this may help.. */
	make_vkbd_irq(vk);
    }
    return c;
}

/* Empties the input buffer of VK. */
static void
vkbd_empty(struct vkbd *vk)
{
    vk->out_pos = vk->in_pos;
    vk->status_byte &= ~KB_STAT_OUTPUT_FULL;
    vk->output_port &= ~KB_OUTP_OUTPUT_FULL;
}

/* Adds the previously read character from the input buffer of VK to the
   end of the input buffer. Note that this may only be called once between
   each call of vkbd_out(). */
static void
vkbd_unout(struct vkbd *vk)
{
    int new_pos = (vk->out_pos - 1) % 16;
    if(new_pos == vk->in_pos)
    {
	/* Previous character has already been overwritten. */
	vkbd_in(vk, KB_REP_OVERRUN);
    }
    else
	vk->out_pos = new_pos;
}


/* I/O port virtualisation. */

static u_long
vkbd_read_port(struct vm *vm, u_short port, int size)
{
    struct vkbd *vk;
    if(size != 1 || vm->tty->kbd_type != Virtual)
	return (u_long)-1;
    vk = &vm->tty->kbd.virtual;
    DB(("vkbd_read_port: vk=%p port=%x\n", vk, port));
    switch(port)
    {
    case 0x60:
	if(vkbd_is_empty(vk))
	{
	    kprintf("vkbd: Someone's reading from an empty buffer.\n");
	    return 0;			/* ?? */
	}
	return vkbd_out(vk) & 0xff;

    case 0x61:
	/* Twiddle the memory-refresh bit (for pcdr). */
	vk->output_8255 ^= 0x10;
	return vk->output_8255;

    case 0x64:
	return vk->status_byte;
    }
    return (u_long)-1;
}

static inline void
enable_vkbd(struct vkbd *vk)
{
    vk->command_byte &= ~KB_CMD_KBD_DISABLED;
    vk->output_8255 |= KB_8255_KBD_ENABLE;
}

static inline void
disable_vkbd(struct vkbd *vk)
{
    vk->command_byte |= KB_CMD_KBD_DISABLED;
    vk->output_8255 &= ~KB_8255_KBD_ENABLE;
}

static inline void
start_speaker(struct vm *vmach)
{
    struct vpit *vp = vpit->get_vpit(vmach);
    if(vp != NULL)
    {
	u_int div = vp->channels[2].divisor;
	if(div == 0)
	    div = 65536;
	tty->speaker_on(vmach->tty, 1193180 / div);
    }
}

/* Called when the divisor of timer channel 2 is altered and the
   timer is gated to the speaker (which is enabled). */
static void
speaker_callback(struct vm *vmach)
{
    start_speaker(vmach);
}

static void
vkbd_write_port(struct vm *vmach, u_short port, int size, u_long val)
{
    struct vkbd *vk;
    if(size != 1 || vmach->tty->kbd_type != Virtual)
	return;
    vk = &vmach->tty->kbd.virtual;
    DB(("vkbd_write_port: vk=%p port=%x val=%x\n", vk, port, val));
    switch(port)
    {
    case 0x61:
	{
	    u_char old = vk->output_8255;
	    vk->output_8255 = val;
	    if((val & KB_8255_KBD_ENABLE) != (old & KB_8255_KBD_ENABLE))
	    {
		if(val & KB_8255_KBD_ENABLE)
		    enable_vkbd(vk);
		else
		    disable_vkbd(vk);
	    }
	    if((val & KB_8255_SPEAKER_ON) != (old & KB_8255_SPEAKER_ON))
	    {
		if(val & KB_8255_SPEAKER_ON)
		{
		    tty->to_front(vmach->tty);
		    if(val & KB_8255_SPEAKER_TIMER)
		    {
			struct vpit *vp = vpit->get_vpit(vmach);
			start_speaker(vmach);
			if(vp != NULL)
			    vp->chan2_callback = speaker_callback;
		    }
		    else
			tty->speaker_on(vmach->tty, 2000);
		}
		if((val & KB_8255_SPEAKER_ON) == 0)
		{
		    struct vpit *vp = vpit->get_vpit(vmach);
		    if(vp != NULL)
			vp->chan2_callback = NULL;
		    tty->speaker_off(vmach->tty);
		}
	    }
	    break;
	}
	    
    case 0x64:
	vk->status_byte |= KB_STAT_COMMAND;
	vk->this_8042_command = 0;
	switch(val)
	{
	case KB_8042_READ_CMD_BYTE:
	    vkbd_in(vk, vk->command_byte);
	    break;

	case KB_8042_WRITE_CMD_BYTE:
	    vk->this_8042_command = val;
	    break;

	case KB_8042_PASSWD_INST:
	case KB_8042_LOAD_PASSWD:
	case KB_8042_ENABLE_PASSWD:
	    /* Unimplemented. */
	    kprintf("vkbd: 8042 passwd handling is unimplemented.\n");
	    break;

	case KB_8042_SELF_TEST:
	    vkbd_in(vk, 0x55);
	    break;

	case KB_8042_INTERFACE_TEST:
	    vkbd_in(vk, 0x00);
	    break;

	case KB_8042_DUMP:
	    /* Unimplemented. */
	    kprintf("vkbd: 8042 status dump is unimplemented.\n");
	    break;

	case KB_8042_DISABLE_KBD:
	    disable_vkbd(vk);
	    break;

	case KB_8042_ENABLE_KBD:
	    enable_vkbd(vk);
	    break;

	case KB_8042_READ_INPUT_PORT:
	    vkbd_in(vk, vk->input_port);
	    break;

	case KB_8042_READ_OUTPUT_PORT:
	    vkbd_in(vk, vk->output_port);
	    break;

	case KB_8042_WRITE_OUTPUT_PORT:
	    vk->this_8042_command = val;
	    break;

	case KB_8042_READ_TEST_INPUTS:
	    /* Unimplemented. */
	    kprintf("vkbd: 8042 read test inputs command is unimplemented.\n");
	    break;

	default:
	    if((val & KB_8042_PULSE_OUTPUT_PORT) == KB_8042_PULSE_OUTPUT_PORT)
	    {
#if 0
		/* Unimplemented. */
		kprintf("vkbd: 8042 pulse output port is unimplemented.\n");
#endif
	    }
	    else
	    {
		kprintf("vkbd: unknown 8042 command: %d.\n", val);
	    }
	}
	break;

    case 0x60:
	vk->status_byte &= ~KB_STAT_COMMAND;
	if(vk->this_8042_command != 0)
	{
	    /* We're half way through an 8042 command. */
	    switch(vk->this_8042_command)
	    {
	    case KB_8042_WRITE_CMD_BYTE:
		vk->command_byte = val;
		break;

	    case KB_8042_WRITE_OUTPUT_PORT:
		if((val & KB_OUTP_RESET_LINE) == 0)
		{
		    /* Task wants to reset. Maybe this should kill the VM? */
		    kprintf("vkbd: Someone's trying the reset line.\n");
		}
		if((val & KB_OUTP_GATE_A20)
		   != (vk->output_port & KB_OUTP_GATE_A20))
		{
		    vm->set_gate_a20(kernel->current_task->user_data,
				     (val & KB_OUTP_GATE_A20) != 0);
		}
		vk->output_port = val;
		break;

	    default:
		kprintf("vkbd: Unexpected second half of 8042 command: %02x, %02x.\n",
			vk->this_8042_command, val);
	    }
	    vk->this_8042_command = 0;
	}
	else if(vk->this_kbd_command != 0)
	{
	    /* We're halfway through a keyboard command. */
	    switch(vk->this_kbd_command)
	    {
	    case KB_KBD_SET_LEDS:
		if(val & 0x80)
		    goto do_kbd_cmd;
		vk->led_state = val;
		if(vk->kbd.in_focus)
		    kbd->set_leds(val);
		vkbd_in(vk, KB_REP_ACK);
		break;

	    case KB_KBD_SET_TYPEMATIC:
		if(val & 0x80)
		    goto do_kbd_cmd;
		/* Unimplemented. */
		kprintf("vkbd: Typematic keys are unimplemented.\n");
		vkbd_in(vk, KB_REP_ACK);
		break;

	    default:
		kprintf("vkbd: Unexpected second half of keyboard command: %02x, %02x\n",
		     vk->this_kbd_command, val);
	    }
	    vk->this_kbd_command = 0;
	}
	else
	{
	do_kbd_cmd:
	    vk->this_kbd_command = 0;
	    switch(val)
	    {
	    case KB_KBD_SET_LEDS:
	    case KB_KBD_SET_TYPEMATIC:
		vkbd_in(vk, KB_REP_ACK);
		vk->this_kbd_command = val;
		break;

	    case KB_KBD_ECHO:
		vkbd_in(vk, KB_REP_ECHO);
		break;

	    case KB_KBD_SEND_ID:
		vkbd_in(vk, 0x83);
		vkbd_in(vk, 0xAB);
		break;

	    case KB_KBD_ENABLE:
	    case KB_KBD_RESET_STOP:
	    case KB_KBD_SET_DEFAULT:
		vkbd_empty(vk);
		vkbd_in(vk, KB_REP_ACK);
		break;

	    case KB_KBD_RESEND:
		vkbd_unout(vk);
		break;

	    case KB_KBD_RESET:
		vkbd_empty(vk);
		vkbd_in(vk, KB_REP_ACK);
		vkbd_in(vk, KB_REP_BAT_OK);
		break;

	    default:
		kprintf("vkbd: Unknown keyboard command: %02x\n", val);
	    }
	}
	break;
    }
}


/* The following tables map the keycodes from 0x60->0x80 to a string of
   scan codes which generate them. All keycodes outside this range are
   their own scan codes. */

static char *plain_e0_down_remap[0x20] =
{
    "\xE0\x1C", "\xE0\x1D", "\xE0\x35", "\xE0\x2A\xE0\x37",	/* 0x60 */
    "\xE0\x38", "\xE0\x46\xE0\xC6", "\xE0\x47", "\xE0\x48",	/* 0x64 */
    "\xE0\x49", "\xE0\x4B", "\xE0\x4D", "\xE0\x4F",		/* 0x68 */
    "\xE0\x50", "\xE0\x51", "\xE0\x52", "\xE0\x53",		/* 0x6c */
    NULL, NULL, NULL, NULL,					/* 0x70 */
    NULL, NULL, NULL, NULL,					/* 0x74 */
    NULL, NULL, NULL, "\xE1\x1D\x45\xE1\x9D\xC5",		/* 0x78 */
    NULL, NULL, NULL, NULL					/* 0x7c */
};

static char *plain_e0_up_remap[0x20] =
{
    "\xE0\x9C", "\xE0\x9D", "\xE0\xB5", "\xE0\xAA\xE0\xB7",	/* 0x60 */
    "\xE0\xB8", NULL, "\xE0\xC7", "\xE0\xC8",			/* 0x64 */
    "\xE0\xC9", "\xE0\xCB", "\xE0\xCD", "\xE0\xCF",		/* 0x68 */
    "\xE0\xD0", "\xE0\xD1", "\xE0\xD2", "\xE0\xD3",		/* 0x6c */
    NULL, NULL, NULL, NULL,					/* 0x70 */
    NULL, NULL, NULL, NULL,					/* 0x74 */
    NULL, NULL, NULL, NULL,					/* 0x78 */
    NULL, NULL, NULL, NULL					/* 0x7c */
};

static char *num_lock_e0_down_remap[0x20] =
{
    "\xE0\x1C", "\xE0\x1D", "\xE0\x35", "\xE0\x2A\xE0\x37",
    "\xE0\x38", "\xE0\x46\xE0\xC6", "\xE0\x2A\xE0\x47", "\x2A\x48\xE0\x48",
    "\xE0\x2A\xE0\x49", "\xE0\x2A\xE0\x4B", "\xE0\x2A\xE0\x4D", "\xE0\x2A\xE0\x4F",
    "\xE0\x2A\xE0\x50", "\xE0\x2A\xE0\x51", "\xE0\x2A\xE0\x52", "\xE0\x2A\xE0\x53",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, "\xE1\x1D\x45\xE1\x9D\xC5",
    NULL, NULL, NULL, NULL
};

static char *num_lock_e0_up_remap[0x20] =
{
    "\xE0\x9C", "\xE0\x9D", "\xE0\xB5", "\xE0\xAA\xE0\xB7",
    "\xE0\xB8", NULL, "\xE0\xC7\xE0\xAA", "\xE0\xC8\xE0\xAA",
    "\xE0\xC9\xE0\xAA", "\xE0\xCB\xE0\xAA", "\xE0\xCD\xE0\xAA", "\xE0\xCF\xE0\xAA",
    "\xE0\xD0\xE0\xAA", "\xE0\xD1\xE0\xAA", "\xE0\xD2\xE0\xAA", "\xE0\xD3\xE0\xAA",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

static char *left_shift_e0_down_remap[0x20] =
{
    "\xE0\x1C", "\xE0\x1D", "\xE0\xAA\xE0\x35", "\xE0\x2A\xE0\x37",
    "\xE0\x38", "\xE0\x46\xE0\xC6", "\xE0\xAA\xE0\x47", "\xAA\x48\xE0\x48",
    "\xE0\xAA\xE0\x49", "\xE0\xAA\xE0\x4B", "\xE0\xAA\xE0\x4D", "\xE0\xAA\xE0\x4F",
    "\xE0\xAA\xE0\x50", "\xE0\xAA\xE0\x51", "\xE0\xAA\xE0\x52", "\xE0\xAA\xE0\x53",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, "\xE1\x1D\x45\xE1\x9D\xC5",
    NULL, NULL, NULL, NULL
};

static char *left_shift_e0_up_remap[0x20] =
{
    "\xE0\x9C", "\xE0\x9D", "\xE0\xB5\xE0\x2A", "\xE0\xAA\xE0\xB7",
    "\xE0\xB8", NULL, "\xE0\xC7\xE0\x2A", "\xE0\xC8\xE0\x2A",
    "\xE0\xC9\xE0\x2A", "\xE0\xCB\xE0\x2A", "\xE0\xCD\xE0\x2A", "\xE0\xCF\xE0\x2A",
    "\xE0\xD0\xE0\x2A", "\xE0\xD1\xE0\x2A", "\xE0\xD2\xE0\x2A", "\xE0\xD3\xE0\x2A",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

static char *right_shift_e0_down_remap[0x20] =
{
    "\xE0\x1C", "\xE0\x1D", "\xE0\xB6\xE0\x35", "\xE0\x2A\xE0\x37",
    "\xE0\x38", "\xE0\x46\xE0\xC6", "\xE0\xB6\xE0\x47", "\xB6\x48\xE0\x48",
    "\xE0\xB6\xE0\x49", "\xE0\xB6\xE0\x4B", "\xE0\xB6\xE0\x4D", "\xE0\xB6\xE0\x4F",
    "\xE0\xB6\xE0\x50", "\xE0\xB6\xE0\x51", "\xE0\xB6\xE0\x52", "\xE0\xB6\xE0\x53",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, "\xE1\x1D\x45\xE1\x9D\xC5",
    NULL, NULL, NULL, NULL
};

static char *right_shift_e0_up_remap[0x20] =
{
    "\xE0\x9C", "\xE0\x9D", "\xE0\xB5\xE0\x36", "\xE0\xAA\xE0\xB7",
    "\xE0\xB8", NULL, "\xE0\xC7\xE0\x36", "\xE0\xC8\xE0\x36",
    "\xE0\xC9\xE0\x36", "\xE0\xCB\xE0\x36", "\xE0\xCD\xE0\x36", "\xE0\xCF\xE0\x36",
    "\xE0\xD0\xE0\x36", "\xE0\xD1\xE0\x36", "\xE0\xD2\xE0\x36", "\xE0\xD3\xE0\x36",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

/* Called each time a keycode is generated and a virtual keyboard is in
   focus. */
static void
vkbd_use_key(struct kbd *keybd, int shift_state, u_char key_code,
	     u_char up_code)
{
    struct vkbd *vk = (struct vkbd *)keybd;
    char *cooked = NULL;
    u_long vk_flags = 0;
    vk->shift_state = shift_state;
    DB(("vkbd_use_key: shift_state=%x key_code=%x up_code=%x\n",
	shift_state, key_code, up_code));
    if(!up_code)
    {
	/* Even though a vkbd doesn't set the leds when it receives
	   the key code of a lock key it has to maintain an idea of which
	   locks are enabled so it can generate the correct scan code
	   sequences. */
	switch(key_code)
	{
	case K_CAPS_LOCK:
	    vk->lock_state ^= KB_LED_CAPS_LOCK;
	    vk_flags |= VK_BUF_SHIFT;
	    break;

	case K_NUM_LOCK:
	    vk->lock_state ^= KB_LED_NUM_LOCK;
	    vk_flags |= VK_BUF_SHIFT;
	    break;

	case K_SCROLL_LOCK:
	    vk->lock_state ^= KB_LED_SCROLL_LOCK;
	    vk_flags |= VK_BUF_SHIFT;
	    break;

	case K_LEFT_CTRL:
	case K_LEFT_SHIFT:
	case K_RIGHT_SHIFT:
	case K_LEFT_ALT:
	case K_E0_RIGHT_CTRL:
	case K_E0_RIGHT_ALT:
	    vk_flags |= VK_BUF_SHIFT;
	    break;
	}
	cooked = kbd->cook_keycode(key_code, shift_state, vk->lock_state);
    }
    else
	vk_flags |= VK_BUF_UP_KEY;
    if(key_code < 0x60)
    {
	/* key_code == scan code */
	vkbd_in(vk, vk_flags | (cooked ? *cooked << 8 : 0)
		| key_code | up_code);
    }
    else
    {
	char *scan_codes;
	if((shift_state & (S_RIGHT_SHIFT | S_LEFT_SHIFT))
	   && (vk->lock_state & L_NUM_LOCK))
	{
	    scan_codes = (up_code
			  ? plain_e0_up_remap
			  : plain_e0_down_remap)[key_code - 0x60];
	}
	else if(vk->lock_state & L_NUM_LOCK)
	{
	    scan_codes = (up_code
			  ? num_lock_e0_up_remap
			  : num_lock_e0_down_remap)[key_code - 0x60];
	}
	else if(shift_state & S_LEFT_SHIFT)
	{
	    scan_codes = (up_code
			  ? left_shift_e0_up_remap
			  : left_shift_e0_down_remap)[key_code - 0x60];
	}
	else if(shift_state & S_RIGHT_SHIFT)
	{
	    scan_codes = (up_code
			  ? right_shift_e0_up_remap
			  : right_shift_e0_down_remap)[key_code - 0x60];
	}
	else
	{
	    scan_codes = (up_code
			  ? plain_e0_up_remap
			  : plain_e0_down_remap)[key_code - 0x60];
	}
	if(scan_codes != NULL)
	{
	    while(*scan_codes != 0)
	    {
		vkbd_in(vk, vk_flags | (cooked ? *cooked << 8 : 0)
			| *scan_codes++);
		cooked = NULL;
	    }
	}
    }
}

/* Called when KBD is switched from. It generates scan codes to simulate all
   the depressed shift keys being released. */
static void
vkbd_switch_from(struct kbd *kbd, int shift_state)
{
    struct vkbd *vk = (struct vkbd *)kbd;
    if(shift_state & S_LEFT_SHIFT)
	vkbd_in(vk, K_LEFT_SHIFT | 0x80);
    if(shift_state & S_RIGHT_SHIFT)
	vkbd_in(vk, K_RIGHT_SHIFT | 0x80);
    if(shift_state & S_LEFT_ALT)
	vkbd_in(vk, K_LEFT_ALT | 0x80);
    if(shift_state & S_RIGHT_ALT)
    {
	vkbd_in(vk, 0xE0);
	vkbd_in(vk, 0xB8);
    }
    if(shift_state & S_LEFT_CTRL)
	vkbd_in(vk, K_LEFT_CTRL | 0x80);
    if(shift_state & S_RIGHT_CTRL)
    {
	vkbd_in(vk, 0xE0);
	vkbd_in(vk, 0x9D);
    }
}

/* Called when KBD is switched to. We have to ensure that it's system has the
   same idea of the current shift state as we do. */
static void
vkbd_switch_to(struct kbd *new, int shift_state)
{
    struct vkbd *vk = (struct vkbd *)new;
    if(shift_state & S_LEFT_SHIFT)
	vkbd_in(vk, K_LEFT_SHIFT);
    if(shift_state & S_RIGHT_SHIFT)
	vkbd_in(vk, K_RIGHT_SHIFT);
    if(shift_state & S_LEFT_ALT)
	vkbd_in(vk, K_LEFT_ALT);
    if(shift_state & S_RIGHT_ALT)
    {
	vkbd_in(vk, 0xE0);
	vkbd_in(vk, 0x18);
    }
    if(shift_state & S_LEFT_CTRL)
	vkbd_in(vk, K_LEFT_CTRL);
    if(shift_state & S_RIGHT_CTRL)
    {
	vkbd_in(vk, 0xE0);
	vkbd_in(vk, 0x1D);
    }
    kbd->set_leds(vk->led_state);
}


/* VBIOS stuff. */

void
vkbd_arpl_handler(struct vm *vm, struct vm86_regs *regs, u_short arpl)
{
    struct vkbd *vk = &vm->tty->kbd.virtual;
    DB(("vkbd_arpl_handler: vk=%p arpl=%x\n", vk, arpl));
    switch(arpl)
    {
    case 0x09:			/* VBIOS keyboard IRQ handler. */
	while(!vkbd_is_empty(vk))
	{
	    u_long c = vkbd_out(vk);
	    if((c & (VK_BUF_UP_KEY | VK_BUF_SHIFT)) == 0)
	    {
		/* Normal make codes only.. */
		u_short head, tail, new_tail;
		head = get_user_short(&BIOS_DATA->kbd_buf_head);
		tail = get_user_short(&BIOS_DATA->kbd_buf_tail);
		new_tail = (tail + 2);
		if(new_tail == get_user_short(&BIOS_DATA->kbd_buf_end))
		    new_tail = get_user_short(&BIOS_DATA->kbd_buf_start);
		if(new_tail == head)
		{
		    tty->beep(vm->tty);
		    break;
		}
		/* Flip the ASCII and scan-code bytes for the BIOS' buffer. */
		c = ((c & 0x00ff) << 8) | ((c & 0xff00) >> 8);
		put_user_short(c, (u_short *)((int)tail + 0x400));
		put_user_short(new_tail, &BIOS_DATA->kbd_buf_tail);
	    }
	}
	put_user_byte(((vk->shift_state & S_RIGHT_SHIFT) ? 1 : 0)
		      | ((vk->shift_state & S_LEFT_SHIFT) ? 2 : 0)
		      | ((vk->shift_state & S_CTRL) ? 4 : 0)
		      | ((vk->shift_state & S_ALT) ? 8 : 0)
		      | ((vk->lock_state & L_SCROLL_LOCK) ? 16 : 0)
		      | ((vk->lock_state & L_NUM_LOCK) ? 32 : 0)
		      | ((vk->lock_state & L_CAPS_LOCK) ? 64 : 0),
		      &BIOS_DATA->kbd_flags[0]);
	put_user_byte(((vk->shift_state & S_LEFT_CTRL) ? 1 : 0)
		      | ((vk->shift_state & S_LEFT_ALT) ? 2 : 0)
		      | ((vk->lock_state & L_SCROLL_LOCK) ? 16 : 0)
		      | ((vk->lock_state & L_NUM_LOCK) ? 32 : 0)
		      | ((vk->lock_state & L_CAPS_LOCK) ? 64 : 0),
		      &BIOS_DATA->kbd_flags[1]);
	if(vk->led_state != vk->lock_state)
	{
	    vk->led_state = vk->lock_state;
	    if(vk->kbd.in_focus)
		kbd->set_leds(vk->led_state);
	}
	break;
    }
}



struct io_handler kbd_data_ioh = {
    NULL, "kbd", 0x60, 0x61, vkbd_read_port, vkbd_write_port
};
struct io_handler kbd_ctrl_ioh = {
    NULL, "kbd", 0x64, 0x64, vkbd_read_port, vkbd_write_port
};

struct arpl_handler kbd_arpl = {
    NULL, "kbd", 0x09, 0x09, vkbd_arpl_handler
};

static bool
vkbd_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
	kbd = (struct kbd_module *)kernel->open_module("kbd", SYS_VER);
	if(kbd != NULL)
	{
	    tty = (struct tty_module *)kernel->open_module("tty", SYS_VER);
	    if(tty != NULL)
	    {
		vpic = (struct vpic_module *)kernel->open_module("vpic", SYS_VER);
		if(vpic != NULL)
		{
		    vpit = (struct vpit_module *)kernel->open_module("vpit", SYS_VER);
		    if(vpit != NULL)
		    {
			vm->add_io_handler(NULL, &kbd_data_ioh);
			vm->add_io_handler(NULL, &kbd_ctrl_ioh);
			vm->add_arpl_handler(&kbd_arpl);
			return TRUE;
		    }
		    kernel->close_module((struct module *)vpic);
		}
		kernel->close_module((struct module *)tty);
	    }
	    kernel->close_module((struct module *)kbd);
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vkbd_expunge(void)
{
    if(vkbd_module.base.open_count == 0)
    {
	vm->remove_arpl_handler(&kbd_arpl);
	vm->remove_io_handler(NULL, &kbd_ctrl_ioh);
	vm->remove_io_handler(NULL, &kbd_data_ioh);
	kernel->close_module((struct module *)vpit);
	kernel->close_module((struct module *)vpic);
	kernel->close_module((struct module *)tty);
	kernel->close_module((struct module *)kbd);
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vkbd_module vkbd_module =
{
    MODULE_INIT("vkbd", SYS_VER, vkbd_init, NULL, NULL, vkbd_expunge),
    init_vkbd
};
