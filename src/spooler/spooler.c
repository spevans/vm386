/* spooler.c -- Printer Spooler */


#include <vmm/kernel.h>
#include <vmm/shell.h>
#include <vmm/tasks.h>
#include <vmm/fs.h>
#include <vmm/spooler.h>
#include <vmm/time.h>
#ifdef	PRINTER_M
#include <vmm/printer.h>
#endif
#include <vmm/lists.h>

#define kprintf kernel->printf

struct fs_module *fs;
#ifdef	PRINTER_M
struct printer_module *printer;
#endif
static bool not_printing = TRUE;
static list_t spool_list;
static struct spool_file handles[MAX_SPOOL_HANDLES];

bool
add_spool_file(char *fname)
{
    static u_int job_number = 1;
    struct file *src, *dst;
    struct spool_entry *new = (struct spool_entry *)
	kernel->malloc(sizeof(struct spool_entry));
    if(new == NULL) return FALSE;
    new->job_no = job_number++;
        new->real_name = (char *)kernel->malloc(strlen(fname)+1);
        if(new->real_name == NULL) return FALSE;
        strcpy(new->real_name, fname);
        if(new_spool_file(new->spool_name) == FALSE) return FALSE;
        kprintf("%s:%s\n", new->real_name, new->spool_name);
        src = fs->open(fname, F_READ);
        if(src != NULL) {
            dst = fs->open(new->spool_name, F_WRITE | F_TRUNCATE | F_CREATE);
            if(dst != NULL) {
                int actual;

                do {
                    int wrote;
                    u_char buf[FS_BLKSIZ];
                    actual = fs->read(&buf, FS_BLKSIZ, src);
                    if(actual < 0)
                        goto error;
                    wrote = fs->write(&buf, actual, dst);
                    if(wrote != actual)
                    {
                    error:
                        fs->close(dst);
                        fs->close(src);
                        return FALSE;
                    }
                } while(actual > 0);
                fs->close(dst);
            }
            else {
                fs->close(src);
                return FALSE;
            }
        }
        else
            return FALSE;
    new->status = QUEUED;
    append_node(&spool_list, &new->node);
    return TRUE;
}


bool
remove_spool_file(unsigned int job)
{
    list_node_t *nxt, *x = spool_list.head;
    struct spool_entry *entry;

    if(job < 1) return FALSE;
    if(list_empty_p(&spool_list) == TRUE) return FALSE;
    while((nxt = x->succ) != NULL) {
        entry = (struct spool_entry *)x;
        if(job == entry->job_no) {
            remove_node(x);
            fs->remove_link(entry->spool_name);
            kernel->free(entry);
            return TRUE;
        }
        x = nxt;
    }
    return FALSE;
}


bool
new_spool_file(char *fname)
{
    struct time_bits tm;

    kernel->expand_time(kernel->current_time(), &tm);
    kernel->sprintf(fname, SPOOL_DIR "%4d%02d%02d%02d%02d%02d.%02d", tm.year,
        tm.month, tm.day, tm.hour, tm.minute, tm.second,
        kernel->get_timer_ticks() & 15); 
    return TRUE;
}

int
open_spool_file(char *name)
{
    int i;
    for(i = 0; (i < MAX_SPOOL_HANDLES) && (handles[i].used == TRUE); i++);
    if(i == MAX_SPOOL_HANDLES) return -1;
    strncpy(handles[i].ident, name, MAX_SPOOL_NAME);
    new_spool_file(handles[i].fname);
    handles[i].fp = fs->open(handles[i].fname, F_WRITE | F_TRUNCATE | F_CREATE);
    if(handles[i].fp == NULL) return -1;
    handles[i].size = 0; 
    handles[i].used = TRUE;
    return i;
}

void
close_spool_file(int handle)
{
    if((handle >= 0) && (handle < MAX_SPOOL_HANDLES)) {
        handles[handle].used = FALSE; 
        fs->close(handles[handle].fp);
        if(handles[handle].size) add_spool_file(handles[handle].fname);
        fs->remove_link(handles[handle].fname);
    }
}

void 
discard_spool_file(int handle)
{
    if((handle >= 0) && (handle < MAX_SPOOL_HANDLES)) {
        handles[handle].used = FALSE; 
        fs->close(handles[handle].fp);
        fs->remove_link(handles[handle].fname);
    }
}

void
write_spool_file(int handle, char *data, int len)
{
    if((handle >= 0) && (handle < MAX_SPOOL_HANDLES) &&
       (handles[handle].used == TRUE)) {
       fs->write(data, len, handles[handle].fp);
       handles[handle].size += len;
    }
}

/* spooler cmds */

#define DOC_lpr "lpr file ...\n\
Add the named files to the spool queue."

int
cmd_lpr(struct shell *sh, int argc, char **argv)
{
    while(argc--) {
        if(add_spool_file(*argv)) {
            sh->shell->printf(sh, "Spooled file %s\n", *argv);
        } else {
            sh->shell->printf(sh, "Failed to spool file %s\n", *argv);
        }
        argv++;
    }
    return 0;
}

#define DOC_lpq "lpq\n\
List the files in the spool queue."

int
cmd_lpq(struct shell *sh, __attribute__ ((unused)) int argc,
        __attribute__ ((unused)) char **argv)
{
    list_node_t *nxt, *x = spool_list.head;
 
    struct spool_entry *entry;
     
    if(list_empty_p(&spool_list) == TRUE) {
        sh->shell->printf(sh, "Printer Spool Queue is empty\n");
    } else {
        sh->shell->printf(sh, "  Job#  Status    File Name\n");
        while((nxt = x->succ) != NULL) {
            entry = (struct spool_entry *)x;
            sh->shell->printf(sh, "%6d  %-8s  %-s\n", entry->job_no,
                (entry->status == QUEUED) ? "queued" : "printing",
                entry->real_name);
            x = nxt;
        }
    }
    return 0;
}

#define DOC_lprm "lprm job# ...\n\
Remove the specified jobs from the spool queue."

int 
cmd_lprm(struct shell *sh, int argc, char **argv)
{
   while(argc--) {
       unsigned int job = kernel->strtoul(*argv, NULL, 10);
       if(remove_spool_file(job)) {
           sh->shell->printf(sh, "Removed job# %d\n", job);
       } else {
           sh->shell->printf(sh, "Failed to remove job# %d\n", job);
       }
       argv++;
   }
   return 0;
}



/* Module stuff. */

struct shell_cmds spooler_cmds =
{
    0,
    { CMD(lpr), CMD(lpq), CMD(lprm), END_CMD }
};

bool
spooler_init(void)
{
    fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
    if(fs == NULL) return FALSE;
#ifdef PRINTER_M
    printer = (struct printer_module *)kernel->open_module("printer", SYS_VER);
#endif
    kernel->add_shell_cmds(&spooler_cmds);
    init_list(&spool_list);
    return TRUE;
}

bool
spooler_expunge(void)
{
    if(spooler_module.base.open_count == 0 && not_printing)
    {
	kernel->close_module((struct module *)fs);
#ifdef PRINTER_M
	kernel->close_module((struct module *)printer);
#endif
	kernel->remove_shell_cmds(&spooler_cmds);
	return TRUE;
    }
    return FALSE;
}
