/* test.c -- Test the low-level handler and the cooked stuff
   John Harper. */

#ifndef GO32
# error Need go32 to test this!
#endif
#include <go32.h>
#include <dpmi.h>

#define __NO_TYPE_CLASHES
#include <vmm/types.h>
#include <vmm/io.h>
#include <vmm/kbd.h>
#include <vmm/keycodes.h>
#include <test/go32prt.h>

static bool kbd_int_flag;
static u_char last_key;

static void
test_int_handler(void)
{
    kbd_int_flag = TRUE;
    outb_p(0x20, 0x20);
}

static void
test_use_key(struct kbd *kbd, int shift_state, u_char key_code, u_char up_code)
{
    kprintf("test_use_key: kbd=%p, shift_state=%#x, key_code=%#x, up_code=%#x\n",
	    kbd, shift_state, key_code, up_code);
    last_key = key_code;
}

static void
test_cooked_func(u_char c)
{
    kprintf("%c", c);
    last_key = c;
}

int
main(int ac, char **av)
{
    _go32_dpmi_seginfo old_handler, new_handler;
    struct cooked_kbd ck;

    setup_screen();

    kprintf("grabbing keyboard interrupt\n");
    _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_handler);
     
    new_handler.pm_offset = (int)test_int_handler;
    new_handler.pm_selector = _go32_my_cs();

    _go32_dpmi_allocate_iret_wrapper(&new_handler);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &new_handler);

    init_cooked_kbd(&ck);
    cooked_set_function(&ck, test_cooked_func);
    kbd_switch(&ck.kbd);
    while(last_key != '\n')
    {
	while(!kbd_int_flag) ;
	kbd_int_flag = FALSE;
	kbd_handler();
    }

    kprintf("releasing keyboard interrupt\n");
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_handler);
    _go32_dpmi_free_iret_wrapper(&new_handler);

    return 0;
}
