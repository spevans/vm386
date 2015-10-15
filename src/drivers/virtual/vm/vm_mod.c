/* vm_mod.c
   John Harper. */

#include <vmm/vm.h>
#include <vmm/kernel.h>
#include <vmm/shell.h>


static bool
vm_init(void)
{
    add_vm_commands();
    add_glue_commands();
    return init_vm();
}

struct vm_module vm_module =
{
    MODULE_INIT("vm", SYS_VER, vm_init, NULL, NULL, NULL),

    create_vm, kill_vm, add_io_handler, remove_io_handler, get_io_handler,
    add_arpl_handler, remove_arpl_handler, get_arpl_handler,
    add_vm_kill_handler,
    alloc_vm_slot, free_vm_slot, set_gate_a20, simulate_vm_int
};

