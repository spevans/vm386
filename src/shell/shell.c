/* shell.c -- Core of the command shell. Perhaps this shell should be
	      called `jsh' :)
   J.S. Harper  */

#ifndef TEST_SHELL
# include <vmm/tasks.h>
# include <vmm/tty.h>
# include <vmm/fs.h>
#else
# define __NO_TYPE_CLASHES
# include <unistd.h>
# include <sys/types.h>
# include <stdio.h>
# include <vmm/types.h>
#endif

#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/fs.h>
#include <vmm/errno.h>
#include <stdarg.h>

#ifndef TEST_SHELL
# define kvsprintf kernel->vsprintf
# define kprintf kernel->printf
#else
# define kprintf printf
# define kvsprintf vsprintf
#include <stdio.h>
#endif

static struct shell sys_shell;

struct shell_module shell_module =
{
    MODULE_INIT("shell", SYS_VER, shell_init, NULL, NULL, NULL),
    shell_print, shell_printf,
    add_command, remove_command, add_cmd_list, remove_cmd_list, arg_error,
    shell_to_front,
    shell_perror,
    spawn_subshell,
    &sys_shell
};


#ifndef TEST_SHELL
static struct tty_module *tty;
static struct fs_module *fs;
static void shell_main(void);
static void shell_kprint(const char *buf, size_t len);
#endif


void
init_shell_struct(struct shell *sh)
{
    strcpy(sh->prompt, "VM/386> ");
    sh->last_rc = 0;
#ifndef TEST_SHELL
    sh->task = NULL;
    sh->tty = NULL;
    sh->src = NULL;
    sh->shell = &shell_module;
#endif
}

bool
shell_init(void)
{
    /* Need to start the shell task? */
#ifndef TEST_SHELL
    /* setup the shell pointer to allow the loading modules to add their
       commands */
    tty = (struct tty_module *)kernel->open_module("tty", SYS_VER);
    if(tty != NULL)
    {
	fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
	if(fs != NULL)
	{
            if (init_cmds(fs, tty))
            {
                init_shell_struct(&sys_shell);
                /* Start it in a suspended state. The shell task must have been
                   created before the tty is allocated, but it can't actually
                   run until the tty has been allocated! */
                sys_shell.task = kernel->add_task(shell_main, 0, 0, "shell");
                if(sys_shell.task != NULL)
                {
                        sys_shell.tty = tty->open_tty(sys_shell.task, Cooked,
                                                      "mda", TRUE);
                        if(sys_shell.tty != NULL)
                        {
                            /* Now we've finished our initialisation procedure
                               start the shell task running properly. */
                            kernel->wake_task(sys_shell.task);
                            kernel->set_print_func(shell_kprint);
                            return TRUE;
                        }
		}
		kernel->kill_task(sys_shell.task);
	    }
	    kernel->close_module((struct module *)fs);
	}
	kernel->close_module((struct module *)tty);
    }
    return FALSE;
#else
    return init_cmds(NULL, NULL);
#endif
}

#ifndef TEST_SHELL
static void
shell_main(void)
{
    source_file(&sys_shell, "/.profile");
    source_file(&sys_shell, "/.shrc");
    while(1)
    {
	shell_loop(&sys_shell);
	shell_printf(&sys_shell, "Can't quit!\n");
    }
}
#endif

void
shell_loop(struct shell *sh)
{
#ifndef TEST_SHELL
    if(sh->task->current_dir == NULL)
    {
	struct file *wd = fs->open("/", F_READ | F_WRITE | F_ALLOW_DIR);
	if(wd != NULL)
	{
	    wd = fs->swap_current_dir(wd);
	    if(wd != NULL)
		fs->close(wd);
	}
    }
#endif
    do {
#ifndef TEST_SHELL
	if(sh->src == NULL)
	{
	    /* Interactive shell. */
	    char *line;
	    size_t length;
	    put_string(sh, sh->prompt);
	    line = tty->readline(sh->tty, &length);
	    if(line != NULL)
	    {
		sh->last_rc = eval_command(sh, line);
		kernel->free(line);
	    }
	    else
		break;
	}
	else
	{
	    /* Non-interactive shell. */
	    char line_buf[256];
	    if(fs->read_line(line_buf, 256, sh->src))
		sh->last_rc = eval_command(sh, line_buf);
	    else
		break;
	}
#else
	char line_buf[256];
	if(isatty(0))
	    put_string(sh, sh->prompt);
	if(!fgets(line_buf, 256, stdin))
	    break;
	if(!isatty(0))
	    put_string(sh, line_buf);
	sh->last_rc = eval_command(sh, line_buf);
#endif
    } while(sh->last_rc != RC_QUIT_NOW);
}

#ifndef TEST_SHELL
static void
subshell_main(void)
{
    struct shell subsh;
    init_shell_struct(&subsh);
    subsh.task = kernel->current_task;
    subsh.tty = tty->open_tty(subsh.task, Cooked, "mda", FALSE);
    if(subsh.tty != NULL)
    {
	source_file(&subsh, "/.shrc");
	shell_loop(&subsh);
	tty->close_tty(subsh.tty);
    }
    else
	kernel->printf("Error: can't open tty\n");
}
#endif /* !TEST_SHELL */

long
spawn_subshell(void)
{
#ifndef TEST_SHELL
    struct task *task = kernel->add_task(subshell_main, TASK_RUNNING,
					 0, "subshell");
    return task ? task->pid : 0;
#else
    return 0;
#endif
}

int
arg_error(struct shell *sh)
{
    put_string(sh, "Error: bad argument(s)\n");
    return(RC_FAIL);
}

void
shell_printf(struct shell *sh, const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    kvsprintf(buf, fmt, args);
    va_end(args);
    put_string(sh, buf);
}

void
shell_print(struct shell *sh, const char *buf, size_t len)
{
#ifndef TEST_SHELL
    if(sh->tty != NULL)
	tty->printn(sh->tty, buf, len);
#else
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
#endif
}

#ifndef TEST
static void
shell_kprint(const char *buf, size_t len)
{
    shell_print(&sys_shell, buf, len);
}
#endif

void
shell_to_front(void)
{
#ifndef TEST_SHELL
    if(sys_shell.tty != NULL)
	tty->to_front(sys_shell.tty);
#endif
}

void
shell_perror(struct shell *sh, const char *msg)
{
    const char *err;
#ifndef TEST_SHELL
    err = kernel->error_string(kernel->current_task->errno);
#elif defined(TEST_FS)
    err = error_string(ERRNO);
#else
    err = NULL;
#endif
    if(err != NULL)
    {
	if(msg)
	    shell_printf(sh, "%s: %s\n", msg, err);
	else
	    shell_printf(sh, "%s\n", err);
    }
}

bool
source_file(struct shell *sh, const char *file_name)
{
#ifndef TEST_SHELL
    struct file *fh;
    fh = fs->open(file_name, F_READ);
    if(fh != NULL)
    {
	struct shell subsh;
	memcpy(&subsh, sh, sizeof(subsh));
	subsh.src = fh;
	shell_loop(&subsh);
	fs->close(fh);
	return TRUE;
    }
#endif
    return FALSE;
}
