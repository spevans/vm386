/* cmds.c -- Shell commands
   John Harper. */

#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/tty.h>
#include <vmm/tasks.h>

#ifndef TEST_SHELL
#include <vmm/fs.h>
#include <vmm/tty.h>
extern struct fs_module *fs;
extern struct tty_module *tty;
#endif

#define DOC_echo "echo ARGS...\n\
Prints all its arguments separated by spaces, followed by a newline."
int
cmd_echo(struct shell *sh, int argc, char **argv)
{
    while(argc--)
    {
	put_string(sh, *argv++);
	if(argc != 0)
	    put_string(sh, " ");
    }
    put_string(sh, "\n");
    return(RC_OK);
}

#define DOC_prompt "prompt PROMPT-STRING\n\
Defines the string printed each time a command line is read."
int
cmd_prompt(struct shell *sh, int argc, char **argv)
{
    if(argc > 0)
    {
	strncpy(sh->prompt, argv[0], PROMPT_SIZE);
	return(RC_OK);
    }
    else
	return(arg_error(sh));
}

#define DOC_help "help [COMMAND]\n\
Prints some text describing either the available commands, or the specific\n\
command COMMAND."
int
cmd_help(struct shell *sh, int argc, char **argv)
{
    if(argc == 0)
    {
	/* Isn't gcc great ;) */
	void callback(const char *name, const char *doc)
	  {  shell_printf(sh, "%-20s", name);  }
	map_commands(callback);
	put_string(sh, "\n\nType `help COMMAND' to display a particular command's documentation.\n");
	return(RC_OK);
    }
    else if(argc == 1)
    {
	const char *doc = find_command_doc(argv[0]);
	if(doc != 0)
	{
	    put_string(sh, doc);
	    put_string(sh, "\n");
	    return(RC_OK);
	}
	return(RC_WARN);
    }
    else
	return(arg_error(sh));
}

#define DOC_quit "quit\n\
Exit the shell. Note that you can only quit from sub-shells, not the\n\
shell created at startup."
int
cmd_quit(struct shell *sh, int argc, char **argv)
{
    return(RC_QUIT_NOW);
}

#define DOC_cls "cls\n\
Clear the screen."
int
cmd_cls(struct shell *sh, int argc, char **argv)
{
#ifndef TEST_SHELL
    if(sh->tty != NULL)
	tty->clear(sh->tty);
#endif
    return 0;
}

#ifndef TEST_SHELL
#define DOC_shell "shell\n\
Create a new shell process in a new virtual console."
int
cmd_shell(struct shell *sh, int argc, char **argv)
{
    long pid;
    pid = spawn_subshell();
    if(pid > 0)
    {
	shell_printf(sh, "[%Z]\n", pid);
	return 0;
    }
    shell_printf(sh, "Error: can't create task\n");
    return RC_FAIL;
}

#define DOC_source "source FILE\n\
. FILE\n\
Execute the shell commands contained in the file FILE."
int
cmd_source(struct shell *sh, int argc, char **argv)
{
    if(argc != 1)
	return 0;
    if(source_file(sh, argv[0]))
	return 0;
    shell_perror(sh, argv[0]);
    return RC_FAIL;
}

#define DOC_ed "ed DEST-FILE\n\
Copies the lines you type at the terminal (upto an end-of-file character)\n\
to the file DEST-FILE."
int
cmd_ed(struct shell *sh, int argc, char **argv)
{
    struct file *fh;
    if(argc != 1)
	return 0;
    fh = fs->open(argv[0], F_WRITE | F_CREATE | F_TRUNCATE);
    if(fh != NULL)
    {
	char *str;
	size_t length;
	while((str = tty->readline(sh->tty, &length)) != NULL)
	{
	    if(fs->write(str, length, fh) != length)
	    {
		shell_perror(sh, argv[0]);
		kernel->free(str);
		break;
	    }
	    kernel->free(str);
	}
	fs->close(fh);
	return 0;
    }
    shell_perror(sh, argv[0]);
    return RC_FAIL;
}
#endif

struct shell_cmds shell_cmds =
{
    0,
    { CMD(echo), CMD(prompt), CMD(help), CMD(quit), CMD(cls),
#ifndef TEST_SHELL
      CMD(shell), CMD(source), { ".", cmd_source, DOC_source }, CMD(ed),
#endif
      END_CMD }
};

bool
init_cmds(void)
{
    add_cmd_list(&shell_cmds);
    return TRUE;
}
