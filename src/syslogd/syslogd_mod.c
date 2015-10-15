/* syslogd_mod.c -- The system log daemon module.
   Simon Evans. */

#include <vmm/types.h>
#include <vmm/syslogd.h>
#include <vmm/kernel.h>


struct syslogd_module syslogd_module =
{
    MODULE_INIT("syslogd", SYS_VER, syslog_init, NULL, NULL, syslog_deinit),
    syslog_entry,
    syslog_cooked_entry,
    syslog_start,
    syslog_stop,
    syslog_status,
    open_syslog,
    close_syslog,
    set_syslog_level
};
