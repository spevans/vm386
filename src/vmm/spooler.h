/* spooler.h -- Definitions for the printer spooler. */

#ifndef __VMM_SPOOLER_H
#define __VMM_SPOOLER_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/lists.h>
#include <vmm/fs.h>

struct file;

/* spooler directory */

#define	SPOOL_DIR	"/spool/"

/* max size of spool file name */
#define MAX_SPOOL_NAME	sizeof(SPOOL_DIR) + 17

#define MAX_SPOOL_HANDLES	64

#define QUEUED		0
#define PRINTING	1

struct spooler_module {
    struct module base;

    bool (*add_spool_file)(char *fname);
    bool (*new_spool_file)(char *fname);
    int (*open_spool_file)(char *name);
    void (*close_spool_file)(int handle);
    void (*discard_spool_file)(int handle);
    void (*write_spool_file)(int handle, char *data, int len);
};

struct spool_file {
    char	ident[MAX_SPOOL_NAME];
    char	fname[MAX_SPOOL_NAME];
    bool	used;
    struct file *fp;
    u_int	size;
};

struct spool_entry {
    list_node_t	node;
    char	*real_name;
    char	spool_name[MAX_SPOOL_NAME+1];
    int		status;
    int		type;
    u_int	job_no;
};

extern bool spooler_init(void);
extern bool spooler_expunge(void);
extern bool add_spool_file(char *fname);
extern bool new_spool_file(char *fname);
extern int open_spool_file(char *name);
extern void close_spool_file(int handle);
extern void discard_spool_file(int handle);
extern void write_spool_file(int handle, char *data, int len);

extern struct spooler_module spooler_module;

#endif /* __VMM_SPOOLER_H */
