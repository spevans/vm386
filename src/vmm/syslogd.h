#ifndef __VMM_SYSLOGD_H__
#define __VMM_SYSLOGD_H__

#include <vmm/types.h>
#include <vmm/kernel.h>
#include <vmm/lists.h>

#define MAX_IDENT_LEN	16
#define MESG_BUF_SIZE   8192
#define MAX_LOG_HANDLES     64

struct session {
  char ident[MAX_IDENT_LEN+1];
  u_int  level;
  bool used;
}; 

struct syslog_status {
  int	total_mesgs;
  int	total_bytes;
  bool	enabled;
  u_int	log_level;
};

#define LOG_NAME "/adm/syslog"

struct syslogd_module {
  struct module base;

  void (*syslog_entry)(int handle, char *entry);
  void (*syslog_cooked_entry)(int handle, char *entry);
  void (*syslog_start)(void);
  void (*syslog_stop)(void);
  void (*syslog_status)(struct syslog_status *status);
  int  (*open_syslog)(char *name, u_int level);
  void (*close_syslog)(int handle);
  void (*set_syslog_level)(u_int level);
};

extern void syslog_task(void);
extern bool syslog_init(void);
extern bool syslog_deinit(void);
extern void syslog_entry(int handle, char *entry);
extern void syslog_cooked_entry(int handle, char *entry);
extern void syslog_start(void);
extern void syslog_stop(void);
extern void syslog_status(struct syslog_status *status);
extern int  open_syslog(char *name, u_int level);
extern void close_syslog(int handle);
extern void set_syslog_level(u_int level);

extern int cmd_syslog(struct shell *sh, int argc, char **argv);
extern bool add_syslogd_cmds(void);

extern struct syslogd_module syslogd_module;

#endif /* __VMM_SYSLOGD_H__ */
