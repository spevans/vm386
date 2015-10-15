/* kbd_mod.c
   John Harper. */

#include <vmm/kbd.h>
#include <vmm/kernel.h>

static bool init_kbd_driver(void);

struct kbd_module kbd_module =
{
    MODULE_INIT("kbd", SYS_VER, init_kbd_driver, NULL, NULL, NULL),
    init_kbd, kbd_switch,
    init_cooked_kbd, cooked_set_function, cooked_set_task, cooked_read_char,
    cook_keycode,
    kbd_set_leds
};


static bool
init_kbd_driver(void)
{
    if(!init_kbd_irq())
	return FALSE;
    return TRUE;
}
