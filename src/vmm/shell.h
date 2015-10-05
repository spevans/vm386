/* shell.h -- definitions for the command-shell thingy.
   J.S. Harper  */

#ifndef _VMM_SHELL_H
#define _VMM_SHELL_H

#include <vmm/module.h>
#include <vmm/string.h>

struct shell {
#ifndef TEST_SHELL
    struct task *task;
    struct tty *tty;
    struct file *src;		/* If non-NULL read commands from here. */
    struct shell_module *shell;
#endif
    int last_rc;
#define PROMPT_SIZE 40
    char prompt[PROMPT_SIZE];
};

struct shell_cmds {
    struct shell_cmds *next;
    struct {
	const char *name;
	int (*func)(struct shell *, int, char **);
	const char *doc;
    } cmds[0];
};

#define CMD(name) __CMD(#name, cmd_ ## name, DOC_ ## name)
#define __CMD(str, fun, doc) { str, fun, doc }
#define END_CMD { 0, 0, 0 }

struct shell_module {
    struct module base;
    void	(*print)(struct shell *sh, const char *, size_t);
    void	(*printf)(struct shell *sh, const char *fmt, ...);
    void	(*add_command)(const char *, int (*)(struct shell *,
						     int, char **),
			       const char *);
    bool	(*remove_command)(const char *);
    void	(*add_cmd_list)(struct shell_cmds *cmds);
    void	(*remove_cmd_list)(struct shell_cmds *cmds);
    int		(*arg_error)(struct shell *sh);
    void	(*shell_to_front)(void);
    void	(*perror)(struct shell *sh, const char *msg);
    long	(*spawn_subshell)(void);
    struct shell *sys_shell;
};

/* Values returned by commands. */
#define RC_OK           0
#define RC_WARN         5
#define RC_FAIL         10
#define RC_NO_CMD       -1
#define RC_QUIT_NOW     0xd1ed1e

#ifdef SHELL_MODULE

/* From shell.c */
extern struct shell_module shell_module;
extern struct shell sys_shell;
#ifndef TEST_SHELL
extern struct tty_module *tty;
#endif
extern void init_shell_struct(struct shell *sh);
extern bool shell_init(void);
extern void shell_loop(struct shell *sh);
extern long spawn_subshell(void);
extern int  arg_error(struct shell *sh);
extern void shell_printf(struct shell *sh, const char *fmt, ...);
extern void shell_print(struct shell *sh, const char *buf, size_t len);
extern void shell_to_front(void);
extern void shell_perror(struct shell *sh, const char *msg);
extern bool source_file(struct shell *sh, const char *file_name);

static inline void
put_string(struct shell *sh, const char *buf)
{
    shell_print(sh, buf, strlen(buf));
}

/* From command.c */
extern int  eval_command(struct shell *sh, char *);
extern void add_command(const char *, int (*)(struct shell *, int, char **),
			const char *);
extern bool remove_command(const char *);
extern void add_cmd_list(struct shell_cmds *cmds);
extern void remove_cmd_list(struct shell_cmds *cmds);
extern const char *find_command_doc(char *);
extern void map_commands(void (*fun)(const char *, const char *));

/* Froms cmds.c */
extern char shell_prompt[];
extern int cmd_echo(struct shell *sh, int, char **);
extern int cmd_prompt(struct shell *sh, int, char **);
extern int cmd_help(struct shell *sh, int, char **);
extern int cmd_quit(struct shell *sh, int, char **);
extern bool init_cmds(void);

#endif /* SHELL_MODULE */

#endif /* _VMM_SHELL_H */
