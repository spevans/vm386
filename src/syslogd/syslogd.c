/* syslog.c -- provides logging of system messages */

#define DEBUG 

#include <vmm/kernel.h>
#include <vmm/fs.h>
#include <vmm/lists.h>
#include <vmm/tasks.h>
#include <vmm/shell.h>
#include <vmm/time.h>
#include <vmm/syslogd.h>
#include <vmm/types.h>

static bool do_logging;
static struct file *LogFp;
static struct fs_module *fs;
static struct task *syslogd;
static struct semaphore new_data;

#define kprintf	kernel->printf
#define printk	kernel->printf

static char *mesg_buf;
static unsigned int buf_idx = 0;
static struct session handles[MAX_LOG_HANDLES];
static u_int log_level = 4;

/* status info */
static int total_mesgs, total_bytes;

void syslog_task(void)
{
  char *p;
#ifdef DEBUG
  LogFp = fs->open(LOG_NAME, F_WRITE | F_CREATE);
  fs->fprintf(LogFp, "syslogd initialised ok!!\n");
  fs->close(LogFp);
#endif

  while(1) {
#if 1
    wait(&new_data);
#endif
    if(buf_idx > 0) {
      LogFp = fs->open(LOG_NAME, F_WRITE | F_CREATE);
      fs->seek(LogFp, 0, SEEK_EOF);
      fs->write(mesg_buf, buf_idx, LogFp);
      fs->close(LogFp);
      p = mesg_buf;
      total_bytes += buf_idx;
      while(buf_idx--) {
        if(*p == '\n') total_mesgs++;
        p++;
      }
      buf_idx = 0;
    }
#if 1
    set_sem_blocked(&new_data);
#endif
  }
}

int
open_syslog(char *name, u_int level)
{
    int i;
    for(i = 0; (i < MAX_LOG_HANDLES) && (handles[i].used == TRUE); i++);
    if(i == MAX_LOG_HANDLES) return -1;
    strncpy(handles[i].ident, name, MAX_IDENT_LEN);
    handles[i].used = TRUE;
    handles[i].level = level;
    return i;
}

void
close_syslog(int handle)
{
    if(handle >= 0 && handle < MAX_LOG_HANDLES) {
        handles[handle].used = FALSE;
    }
}


void 
set_syslog_level(u_int level)
{
    log_level = level;
}

bool syslog_init(void)
{
  mesg_buf = (char *)kernel->malloc(MESG_BUF_SIZE);
  if(mesg_buf == NULL) {
       DB(("couldnt alloc buffer\n"));
       return FALSE;
  }
  fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
  if(fs != NULL) {
    strcpy(handles[0].ident, "kernel");
    handles[0].level = 0;
    handles[0].used = TRUE;
    do_logging = TRUE;
    set_sem_blocked(&new_data);
    syslogd = kernel->add_task(syslog_task, TASK_RUNNING, 0, "syslogd");
    add_syslogd_cmds();
    DB(("syslogd init ok\n"));
    return TRUE;
  } else {
    DB(("couldnt open fs\n"));
  }
  return FALSE;
}


bool syslog_deinit(void)
{
  if(syslogd_module.base.open_count == 0) {
    do_logging = FALSE;
    kernel->close_module((struct module *)fs);
    return TRUE;
  }
  return FALSE;
}


static bool
check_handle(int handle)
{
  if(handle < 0 || handle >= MAX_LOG_HANDLES) return FALSE;
  if(handles[handle].used == FALSE) return FALSE;
  if(handles[handle].level > log_level) return FALSE;
  return TRUE;
}

/* direct from kernel's printf */
void syslog_cooked_entry(int handle, char *entry)
{
    int len;

    if(do_logging == FALSE) return;
    if(check_handle(handle) == FALSE) return;

    len = strlen(entry);

    forbid();
    if((buf_idx + len +1) < MESG_BUF_SIZE) {
        strcpy(mesg_buf + buf_idx, entry);
        buf_idx += len;
    }
    permit();
#if 1
    signal(&new_data);
#endif
}


void syslog_entry(int handle, char *entry)
{
  char lentry[1024];
  struct time_bits tm;
    
  if(do_logging == FALSE) return;
  if(check_handle(handle) == FALSE) return;

  kernel->expand_time(kernel->current_time(), &tm);
  kernel->sprintf(lentry, "%3s %-2d %2d:%02d:%02d: %s: %s\n", tm.month_abbrev,
      tm.day, tm.hour, tm.minute, tm.second, handles[handle].ident, entry);
  syslog_cooked_entry(handle, lentry);
}

void syslog_start(void)
{
  do_logging = TRUE;
}

void syslog_stop(void)
{
  do_logging = FALSE;
}

void syslog_status(struct syslog_status *status)
{
    status->total_mesgs = total_mesgs;
    status->total_bytes = total_bytes;
    status->enabled = do_logging;
    status->log_level = log_level;
}
