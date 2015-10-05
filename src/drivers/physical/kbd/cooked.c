/* cooked.c -- ASCII interface to the keyboard. Mainly for the shell.
   John Harper. */

#include <vmm/types.h>
#include <vmm/kbd.h>
#include <vmm/keycodes.h>
#include <vmm/tasks.h>
#include <vmm/string.h>

#ifdef TEST_KBD
# ifndef GO32
#  error Need go32
# endif
# include <test/go32prt.h>
#else
# include <vmm/kernel.h>
# define kprintf kernel->printf
#endif

#include "keymaps/gb.h"

static void cooked_use_key(struct kbd *kbd, int shift_state, u_char key_code,
			   u_char up_code);
static void cooked_switch_to(struct kbd *kbd, int shift_state);
static void cooked_switch_from(struct kbd *kbd, int shift_state);


/* Initialise the cooked keyboard structure pointed to by CK. The CK->func
   field is set to NULL as well. */
void
init_cooked_kbd(struct cooked_kbd *ck)
{
    init_kbd(&ck->kbd);
    ck->kbd.use_key = cooked_use_key;
    ck->kbd.switch_to = cooked_switch_to;
    ck->kbd.switch_from = cooked_switch_from;
    ck->lock_state = 0;
    ck->type = ct_None;
}

/* Sets the function field of CK to FUNC. This is the only supported way
   of setting this field! */
void
cooked_set_function(struct cooked_kbd *ck, void (*func)(u_char))
{
    ck->type = ct_Func;
    ck->receiver.func = func;
}

/* Set the keyboard CK to buffer its output, so that the function
   cooked_read_char() can be used to read characters. */
void
cooked_set_task(struct cooked_kbd *ck)
{
    ck->type = ct_Task;
}

static inline void
toggle_lock_state(struct cooked_kbd *ck, int lock_mask)
{
    ck->lock_state ^= lock_mask;
    if(&ck->kbd == kbd_focus)
	kbd_set_leds(ck->lock_state);
}

static inline char **
find_keymap(int shift_state, int lock_state)
{
    int km_index = 0;
    if(lock_state & L_CAPS_LOCK)
	km_index = KM_SHIFT;
    if(lock_state & L_NUM_LOCK)
	km_index |= KM_NUM_LOCK;
    if(shift_state & S_SHIFT)
	km_index ^= KM_SHIFT;
    if(shift_state & S_ALT)
	km_index |= KM_ALT;
    if(shift_state & S_CTRL)
	km_index |= KM_CTRL;
    return keymap[km_index];
}

/* Return the string that KEY-CODE, SHIFT-STATE & LOCK-STATE cook to. */
inline char *
cook_keycode(u_char key_code, int shift_state, int lock_state)
{
    char **map = find_keymap(shift_state, lock_state);
    return map ? map[key_code] : NULL;
}
    
/* Called by the kbd q'd irq handler when a key code is available in a cooked
   console. */
static void
cooked_use_key(struct kbd *kbd, int shift_state, u_char key_code,
	       u_char up_code)
{
    struct cooked_kbd *ck = (struct cooked_kbd *)kbd;
    DB(("cuk: kbd=%p, shift_state=%#x, key_code=%#x, up_code=%#x\n",
	kbd, shift_state, key_code, up_code));
    if(up_code == 0)
    {
	char *str;
	switch(key_code)
	{
	case K_CAPS_LOCK:
	    DB(("cuk: toggling caps_lock\n"));
	    toggle_lock_state(ck, L_CAPS_LOCK);
	    break;

	case K_NUM_LOCK:
	    DB(("cuk: toggling num_lock\n"));
	    toggle_lock_state(ck, L_NUM_LOCK);
	    break;

	case K_SCROLL_LOCK:
	    DB(("cuk: toggling scroll_lock\n"));
	    toggle_lock_state(ck, L_SCROLL_LOCK);
	    break;
	}
	str = cook_keycode(key_code, shift_state, ck->lock_state);
	if(str != NULL)
	{
	    switch(ck->type)
	    {
	    case ct_Func:
		DB(("cuk: feeding `%s' to function %p\n", str, ck->func));
		while(*str)
		    ck->receiver.func(*str++);
		break;

	    case ct_Task:
	    {
		int len = strlen(str);
		while(len-- > 0)
		{
		    int new_in = ((ck->receiver.task.in + 1)
				  % COOKED_BUFSIZ);
		    if(new_in == ck->receiver.task.out)
			break;
		    ck->receiver.task.buf[ck->receiver.task.in] = *str++;
		    ck->receiver.task.in = new_in;
		}
		if(ck->receiver.task.task != NULL)
		{
		    kernel->wake_task(ck->receiver.task.task);
		    ck->receiver.task.task = NULL;
		}
		break;
	    }

	    case ct_None:
		break;
	    }
	}
    }
}

/* Called by the task owning CK to sleep until a character is available,
   then return that character. cooked_set_task() must have been called
   on CK for this to work. */
u_char
cooked_read_char(struct cooked_kbd *ck)
{
    u_char c;
    if(ck->type != ct_Task)
	return 0;
    while(ck->receiver.task.in == ck->receiver.task.out)
    {
	/* Buffer empty. */
	ck->receiver.task.task = kernel->current_task;
	kernel->suspend_current_task();
    }
    c = ck->receiver.task.buf[ck->receiver.task.out];
    ck->receiver.task.out = (ck->receiver.task.out + 1) % COOKED_BUFSIZ;
    return c;
}

static void
cooked_switch_from(struct kbd *kbd, int shift_state)
{
}

static void
cooked_switch_to(struct kbd *kbd, int shift_state)
{
    struct cooked_kbd *ck = (struct cooked_kbd *)kbd;
    kbd_set_leds(ck->lock_state);
}
