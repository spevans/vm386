/* command.c -- command handling in the shell
   J.S. Harper  */

#include <vmm/shell.h>
#include <vmm/io.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#ifndef TEST_SHELL
# include <vmm/tasks.h>
# define malloc kernel->malloc
# define free kernel->free
# define kprintf kernel->printf
#else
# define forbid() do ; while(0)
# define permit() do ; while(0)
#endif


/* Each command gets one of these structures. */
struct command {
    struct command *next;
    const char	   *name;
    unsigned int    hash_value;
    /* Function pointer to the code of this command. It gets a vector of
       ARGC argument strings in ARGV. */
    int           (*func)(struct shell *sh, int argc, char **argv);
    const char	   *doc;
};

/* This is a block of commands; grouped together for allocation purposes. */
#define CMD_BLK_SIZE 32
struct command_block {
    struct command_block *next;
    struct command cmds[CMD_BLK_SIZE];
};


/* Produce a hash value from the string STRING. This hash function should
   be good enough; but for the number of command names being hashed it
   doesn't really matter anyway... */
static inline unsigned int
hash_string(register const char *string)
{
    register unsigned int value = 0;
    while(*string)
	value = (value * 33) + *string++;
    return(value);
}


/* Allocation of command structures. */

static struct command *free_list;
static struct command_block *block_list;

static struct command *
alloc_command(void)
{
    struct command *cmd;
    forbid();
    if((cmd = free_list) == NULL)
    {
	int i;
	struct command_block *new = malloc(sizeof(struct command_block));
	new->next = block_list;
	block_list = new;
	for(i = 0; i < (CMD_BLK_SIZE - 1); i++)
	    new->cmds[i].next = &new->cmds[i+1];
	new->cmds[i].next = NULL;
	free_list = new->cmds;
	cmd = free_list;
    }
    free_list = cmd->next;
    permit();
    return cmd;
}

static inline void
free_command(struct command *cmd)
{
    forbid();
    cmd->next = free_list;
    free_list = cmd;
    permit();
}


/* Management of the hash table. */

#define CMD_HASH_SIZE 31

static struct command *command_table[CMD_HASH_SIZE];

/* Add a new command called NAME (calling FUNC) to the hash table of
   commands. Note that command structures are allocated from a finite
   pool which may be filled if it isn't big enough. (See MAX_COMMANDS).
   DOC is a string (or NULL) documenting this command. */
void
add_command(const char *name, int (*func)(struct shell *, int, char **),
	    const char *doc)
{
    struct command *new_cmd = alloc_command();
    new_cmd->name = name;
    new_cmd->func = func;
    new_cmd->doc = doc;
    new_cmd->hash_value = hash_string(name);
    forbid();
    new_cmd->next = command_table[new_cmd->hash_value % CMD_HASH_SIZE];
    command_table[new_cmd->hash_value % CMD_HASH_SIZE] = new_cmd;
    permit();
}

/* Remove the command called NAME. Returns TRUE is it succeeded, FALSE
   otherwise. */
bool
remove_command(const char *name)
{
    unsigned int hash_val = hash_string(name);
    struct command **itemp;
    forbid();
    itemp = &command_table[hash_val % CMD_HASH_SIZE];
    while(*itemp != NULL)
    {
	if(((*itemp)->hash_value == hash_val)
	   && !strcmp((*itemp)->name, name))
	{
	    struct command *item = *itemp;
	    *itemp = (*itemp)->next;
	    free_command(item);
	    permit();
	    return TRUE;
	}
	itemp = &(*itemp)->next;
    }
    permit();
    return FALSE;
}

void
add_cmd_list(struct shell_cmds *cmds)
{
    u_int i;
    for(i = 0; cmds->cmds[i].name != NULL; i++)
	add_command(cmds->cmds[i].name, cmds->cmds[i].func, cmds->cmds[i].doc);
}

void
remove_cmd_list(struct shell_cmds *cmds)
{
    u_int i;
    for(i = 0; cmds->cmds[i].name != NULL; i++)
	remove_command(cmds->cmds[i].name);
}

/* Search the hash table for a command called NAME. Return its command
   structure or the null pointer if no match is found. */
static struct command *
find_command(const char *name)
{
    unsigned int hash_val = hash_string(name);
    struct command *item;
    forbid();
    item = command_table[hash_val % CMD_HASH_SIZE];
    while(item != NULL)
    {
	if((item->hash_value == hash_val)
	   && (strcmp(item->name, name) == 0))
	{
	    /* A match! */
	    break;
	}
	item = item->next;
    }
    permit();
    return item;
}

/* Returns the function or NULL for the command named NAME. */
void *
find_command_func(const char *name)
{
    struct command *cmd;
    void *func;
    forbid();
    cmd = find_command(name);
    func = cmd ? cmd->func : NULL;
    permit();
    return func;
}

/* Returns the doc-string or NULL for the command named NAME. */
const char *
find_command_doc(char *name)
{
    struct command *cmd;
    const char *doc;
    forbid();
    cmd = find_command(name);
    doc = cmd ? cmd->doc : NULL;
    permit();
    return doc;
}

/* Call the function FUN on each command in the hash table; it gets
   called with two arguments: the name and doc string of the command. */
void
map_commands(void (*fun)(const char *, const char *))
{
    int i;
    forbid();
    for(i = 0; i < CMD_HASH_SIZE; i++)
    {
	struct command *cmd = command_table[i];
	while(cmd != NULL)
	{
	    fun(cmd->name, cmd->doc);
	    cmd = cmd->next;
	}
    }
    permit();
}


/* Evaluating a command line. */

/* max # of args to a command */
#define MAX_ARGS 10

/* Takes a line of input, explodes each word in it, then calls the command
   named in the first word with the other words as its argument values.
   Returns the value returned by the command or RC_NO_CMD if the command
   can't be found. Note that INPUT-LINE will be modified. */
int
eval_command(struct shell *sh, char *input_line)
{
    char *args[MAX_ARGS];
    char *ptr = input_line;
    int i;
    struct command *cmd;

    /* First strip out each word. */
    for(i = 0; *ptr && (i < MAX_ARGS); i++)
    {
	char *start;
	/* Miss leading whitespace. */
	while(*ptr && ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')))
	    ptr++;
	/* Step over the token. */
	if(*ptr == '\'')
	{
	    /* A quoted string. Simply scan for the next ' character. */
	    start = ++ptr;
	    while(*ptr && (*ptr != '\''))
		ptr++;
	}
	else if((*ptr == 0) || (*ptr == '#'))
	{
	    args[i] = NULL;
	    break;
	}
	else
	{
	    /* A whitespace-delimited string. */
	    start = ptr;
	    while(*ptr && (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n'))
		ptr++;
	}
	/* This check ensures we don't miss the end of the input string. */
	if(*ptr != 0)
	  *ptr++ = 0;
	args[i] = start;
    }

    if(i == 0 || **args == 0)
    {
	/* null command. */
	return(RC_OK);
    }
    else
    {
	/* Now we've exploded the arg string the name of the command is in
	   args[0] and each arg value args[1..i] Next call the command. */
	cmd = find_command(args[0]);
	if(cmd != 0)
	    return(cmd->func(sh, i - 1, args + 1));
	else
	{
	    put_string(sh, "Error: unknown command `");
	    put_string(sh, args[0]);
	    put_string(sh, "'\n");
	    return(RC_NO_CMD);
	}
    }
}
