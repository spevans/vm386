/* cmds.c -- the shell commands for the floppy driver
 * C.Luke
 */

#include "fd.h"

#include <vmm/shell.h>
#include <vmm/string.h>

// Most of these commands are for testing so they ignore the parameters
#pragma GCC diagnostic ignored "-Wunused-parameter"


#undef kprintf
#define kprintf sh->shell->printf

#define DOC_fdinfo "fdinfo\n\
Print information about the floppy-disk driver."
int cmd_fdinfo(struct shell *sh, int argc, char *argv[])
{
    return 0;
}

#define DOC_fdseek "fdseek CYLINDER\n\
Force the first floppy drive to seek to the specified cylinder."
int cmd_fdseek(struct shell *sh, int argc, char *argv[])
{
	u_long cyl;

	if(argc != 1)
	{
		kprintf(sh, "Syntax: %s\n", DOC_fdseek);
		return 0;
	}

	cyl = kernel->strtoul(argv[0], NULL, 0);

	kprintf(sh, "Seeking to cylinder: %d\n", cyl);

	floppy_force_seek(NULL, cyl);

	return 0;
}

#define DOC_fdread "fdread BLOCK BLOCKS\n\
Read BLOCKS number of blocks starting from block BLOCK."
int cmd_fdread(struct shell *sh, int argc, char *argv[])
{
	u_long blk;
	int blks;
	char *buf;

	if(argc != 2)
	{
		kprintf(sh, "Syntax: %s\n", DOC_fdread);
		return 0;
	}

	blk = kernel->strtoul(argv[0], NULL, 0);
	blks = kernel->strtoul(argv[1], NULL, 0);
	buf = kernel->malloc(blks * 1024);

	kprintf(sh, "Reading %d blocks from %lu to %p\n", blks, blk, buf);

	kprintf(sh, "Returns: %l\n",
		floppy_read_blocks(&fd_devs[0], buf, blk, blks));

	return 0;
}

#define DOC_fdreset "fdreset\nReset fdc"
int cmd_fdreset(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "reset_fdc()...\n");
	kprintf(sh, "reset_fdc() returned: %d\n", reset_fdc());
	dump_stat();
	return 0;
}

#define DOC_fdadd "fdadd\nAdd floppy drive to FS list"
int cmd_fdadd(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "Adding floppy device 0...\n");
	floppy_add_dev(&fd_devs[0]);
	kprintf(sh, "Added.\n");
	return 0;
}

#define DOC_fdrecal "fdrecal\nRecalibrate fdc"
int cmd_fdrecal(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "fdc_recal()...\n");
	kprintf(sh, "fdc_recal() returned: %d\n", fdc_recal(&fd_devs[0]));
	dump_stat();
	return 0;
}

#define DOC_fdid "fdid\nRead floppy ID"
int cmd_fdid(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "fdc_read_id()...\n");
	kprintf(sh, "fdc_read_id() returned: %d\n", fdc_read_id(&fd_devs[0]));
	dump_stat();
	return 0;
}

#define DOC_fdspecify "fdspecify\nSend specify command to fdc"
int cmd_fdspecify(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "fdc_specify()...\n");
	kprintf(sh, "fdc_specify() returned: %d\n", fdc_specify(&fd_devs[0]));
	dump_stat();
	return 0;
}

#define DOC_fdsense "fdsense\nSend sense command to fdc"
int cmd_fdsense(struct shell *sh, int argc, char *argv[])
{
	kprintf(sh, "fdc_sense()...\n");
	kprintf(sh, "fdc_sense() returned: %d\n", fdc_sense());
	dump_stat();
	return 0;
}


struct shell_cmds fd_cmds =
{
	0,
	{ CMD(fdinfo), CMD(fdseek), CMD(fdread), CMD(fdadd),
	  CMD(fdrecal), CMD(fdspecify), CMD(fdid), CMD(fdreset),
	  CMD(fdsense), END_CMD
	},
};

bool add_floppy_commands(void)
{
	kernel->add_shell_cmds(&fd_cmds);
	return TRUE;
}

void remove_floppy_commands(void)
{
	kernel->remove_shell_cmds(&fd_cmds);
}

