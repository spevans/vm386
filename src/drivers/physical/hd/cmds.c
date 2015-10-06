/* cmds.c -- Hard disk shell commands.
   John Harper. */

#include <vmm/types.h>
#include <vmm/hd.h>
#include <vmm/shell.h>
#include <vmm/fs.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>

#define DOC_hdinfo "hdinfo\n\
Print information about the hard-disk driver."
int
cmd_hdinfo(struct shell *sh, __attribute__((unused)) int argc,
           __attribute__((unused)) char **argv)
{
    hd_dev_t *dev;
    hd_partition_t *p;
    sh->shell->printf(sh, "%-8s  %10s  %10s  %10s\n", "Name", "Blocks", 
		      "Read", "Write");
    forbid();
    dev = hd_list;
    while(dev != NULL)
    {
	sh->shell->printf(sh, "%-8s  %10d  %10x  %10x\n", dev->name,
			  dev->total_blocks, dev->read_blocks,
			  dev->write_blocks);
	dev = dev->next;
    }
    sh->shell->printf(sh, "\n%-8s  %-10s  %10s  %10s\n", "Name", "Device",
		      "Start", "Size");
    p = partition_list;
    while(p != NULL)
    {
	sh->shell->printf(sh, "%-8s  %-10s  %10d  %10d\n", p->name,
			  p->hd->name, p->start, p->size);
	p = p->next;
    }
    permit();
    return 0;
}

#define DOC_hdperf "hdperf PARTITION BLOCKS [BLOCKS-PER-REQUEST]\n\
Time the hard-disk driver reading BLOCKS number of blocks from the\n\
partition called PARTITION."
int
cmd_hdperf(struct shell *sh, int argc, char **argv)
{
    hd_partition_t *hd;
    struct fs_device *dev;
    u_long blocks_to_read, blocks_per_req;
    if(argc < 2)
	return 0;
    blocks_to_read = kernel->strtoul(argv[1], NULL, 0);
    blocks_per_req = (argc >= 3) ? kernel->strtoul(argv[2], NULL, 0) : 2;
    if(blocks_per_req == 0)
	return 0;
    hd = hd_find_partition(argv[0]);
    if(hd != NULL)
    {
	u_long time;
	u_long blkno = 0;
	sh->shell->printf(sh, "Raw device: ");
	time = kernel->get_timer_ticks();
	while(blkno < (blocks_to_read * blocks_per_req))
	{
	    u_char buf[512 * blocks_per_req];
	    if(!hd_read_blocks(hd, buf, blkno, blocks_per_req))
	    {
		time = kernel->get_timer_ticks() - time;
		sh->shell->printf(sh, "Error: can't read block %d\n", blkno);
		goto end;
	    }
	    blkno += blocks_per_req;
	}
	time = kernel->get_timer_ticks() - time;
    end:
	sh->shell->printf(sh, "%u %d-byte blocks in %u ticks\n",
			  blkno/2, blocks_per_req * 512, time);
    }
    else
	sh->shell->printf(sh, "Error: no partition `%s'\n", argv[0]);
    dev = fs->get_device(argv[0]);
    if(dev != NULL)
    {
	u_long time;
	u_long blkno = 0;
	sh->shell->printf(sh, "Buffer-cache: ");
	time = kernel->get_timer_ticks();
	while(blkno < blocks_to_read)
	{
	    struct buf_head *bh = fs->bread(dev, blkno);
	    if(bh == NULL)
	    {
		time = kernel->get_timer_ticks() - time;
		sh->shell->printf(sh, "Error: can't read block %d\n", blkno);
		goto end2;
	    }
	    fs->brelse(bh);
	    blkno++;
	}
	time = kernel->get_timer_ticks() - time;
    end2:
	sh->shell->printf(sh, "%u 1024-byte blocks in %u ticks\n",
			  blkno, time);
    }
    else
	sh->shell->printf(sh, "Error: no device `%s'\n", argv[0]);
    return 0;
}

struct shell_cmds hd_cmds =
{
    0, { CMD(hdinfo), CMD(hdperf), END_CMD }
};

bool
add_hd_commands(void)
{
    kernel->add_shell_cmds(&hd_cmds);
    return TRUE;
}
