/* cmds.c -- Shell commands to do syslogd.
   Simon Evans. */

#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/types.h>
#include <vmm/syslogd.h>

#define DOC_syslog "syslog OPTIONS...\n\
Controls the operation of the syslog daemon.\n\
Options available are:\n\n\
	'-stop'		Stop logging messages.\n\
	'-start'	Start logging messages.\n\
	'-status'	Show current syslog status.\n\
	'-level LEVEL'	Setl logging level."
int
cmd_syslog(struct shell *sh, int argc, char **argv)
{
    char *fname = NULL;
    bool stop = FALSE, start = FALSE, status = FALSE, level = FALSE;

    while(argc--) {
        if(**argv != '-') {
            fname = *argv;
            break;
        } else {
            if(!strcmp(*argv, "-start"))
                start = TRUE;
            else if(!strcmp(*argv, "-stop"))
                stop = TRUE;
            else if(!strcmp(*argv, "-status"))
                status = TRUE;
            else if(!strcmp(*argv, "-level"))
                level = TRUE;   
            else {
                sh->shell->printf(sh, "Unknown option: `%s'\n", *argv);
                return 1;
            }
        }
        argv++;
    }
    if(start && stop) { /* check for bozos */
        sh->shell->printf(sh, "Start or stop, which one? go outside "
	    "have a fag, and come back when you know\n"
            "what you are doing.\n");
        return 1;
    }
    if(stop) syslog_stop();
    if(start) syslog_start();
    if(status) {
        struct syslog_status status;

        syslog_status(&status);
        sh->shell->printf(sh, "syslog status:\n");
        sh->shell->printf(sh, "Logging %s\tTotal Messages: %d\tTotal Bytes: %d\n",
            status.enabled ? "enabled" : "disabled",
            status.total_mesgs, status.total_bytes);
        sh->shell->printf(sh, "Logging Level: %d\n", status.log_level); 
    }
    if(level) set_syslog_level(kernel->strtoul(*(argv+1), NULL, 10));
    return 0;
}

struct shell_cmds syslogd_cmds =
{
    0,
    { CMD(syslog), END_CMD }
};

bool
add_syslogd_cmds(void)
{
    kernel->add_shell_cmds(&syslogd_cmds);
    return TRUE;
}
