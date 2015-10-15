/* spooler.c -- Printer Spooler */

#include <vmm/kernel.h>
#include <vmm/spooler.h>


struct spooler_module spooler_module =
{
    MODULE_INIT("spooler", SYS_VER, spooler_init, NULL, NULL, spooler_expunge),
    add_spool_file,
    new_spool_file,
    open_spool_file,
    close_spool_file,
    discard_spool_file,
    write_spool_file
};
