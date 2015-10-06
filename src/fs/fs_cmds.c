/* fs_cmds.c -- Shell commands to access the file system.
   John Harper. */

#ifdef TEST
# include <unistd.h>
# include <fcntl.h>
# include <stdio.h>
#endif

#include <vmm/types.h>
#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/hd.h>
#include <vmm/time.h>
#include <vmm/shell.h>
#include <vmm/string.h>
#include <vmm/kernel.h>

#ifndef TEST
# define current_time kernel->current_time
# define expand_time kernel->expand_time
# define SHELL sh->shell
#else
# define SHELL shell
#endif

#define DOC_cp "cp SOURCE-FILE DEST-FILE\n\
Copy the file SOURCE-FILE to the file DEST-FILE."
int
cmd_cp(struct shell *sh, int argc, char **argv)
{
    int rc = RC_FAIL;
    if(argc == 2)
    {
	struct file *src = open_file(argv[0], F_READ);
	if(src != NULL)
	{
	    struct file *dst = open_file(argv[1],
					 F_WRITE | F_TRUNCATE | F_CREATE);
	    if(dst != NULL)
	    {
		int actual;
		rc = RC_OK;
		do {
		    int wrote;
		    u_char buf[FS_BLKSIZ];
		    actual = read_file(&buf, FS_BLKSIZ, src);
		    if(actual < 0)
			goto error;
		    wrote = write_file(&buf, actual, dst);
		    if(wrote != actual)
		    {
		    error:
			SHELL->perror(sh, argv[0]);
			rc = RC_FAIL;
			break;
		    }
		} while(actual > 0);
		close_file(dst);
	    }
	    else
		SHELL->perror(sh, argv[1]);
	    close_file(src);
	}
	else
	    SHELL->perror(sh, argv[0]);
    }
    else
	rc = SHELL->arg_error(sh);
    return rc;
}

#define DOC_type "type FILES...\n\
Print the contents of the file called FILE."
int
cmd_type(struct shell *sh, int argc, char **argv)
{
    int rc = RC_OK;
    while(rc == RC_OK && argc > 0)
    {
	struct file *f = open_file(*argv, F_READ);
	if(f != NULL)
	{
	    int actual;
	    do {
		u_char buf[FS_BLKSIZ];
		actual = read_file(&buf, FS_BLKSIZ, f);
		if(actual < 0)
		{
		    SHELL->perror(sh, argv[0]);
		    rc = RC_FAIL;
		    break;
		}
		SHELL->print(sh, (char *)&buf, actual);
	    } while(actual > 0);
	    close_file(f);
	    argc--; argv++;
	}
	else
	{
	    SHELL->perror(sh, *argv);
	    rc = RC_FAIL;
	}
    }
    return rc;
}

static char *
decode_attributes(u_long attr)
{
    static char buf[16];
    char *ptr = buf;
    *ptr++ = (attr & ATTR_DIRECTORY) ? 'd' : ((attr & ATTR_SYMLINK) ? 'l' : '-');
    *ptr++ = (attr & ATTR_NO_READ) ? '-' : 'r';
    *ptr++ = (attr & ATTR_NO_WRITE) ? '-' : 'w';
    *ptr++ = (attr & ATTR_EXEC) ? 'x' : '-';
    *ptr++ = 0;
    return buf;
}

#define DOC_ls "ls [OPTIONS...] [DIRECTORY]\n\
Prints a listing of the directory DIRECTORY (or the current directory)\n\
using the options OPTIONS (currently there are no options!)."
int
cmd_ls(struct shell *sh, int argc, char **argv)
{
    int rc = RC_OK;
    struct file *dir;
    int entries;
    if(argc == 1)
	dir = open_file(argv[0], F_READ | F_ALLOW_DIR);
    else
	dir = get_current_dir();
    if(dir && !F_IS_DIR(dir))
    {
	close_file(dir);
	dir = NULL;
	ERRNO = E_NOTDIR;
    }
    if(dir == NULL)
    {
	SHELL->perror(sh, argc == 1 ? argv[0] : ".");
	return RC_FAIL;
    }
    while(1)
    {
	int i;
	struct dir_entry_blk dir_blk;
	entries = read_file(&dir_blk, FS_BLKSIZ, dir);
	if(entries < 0)
	    goto error;
	if(entries == 0)
	    goto end;
	entries /= sizeof(struct dir_entry);
	for(i = 0; i < entries; i++)
	{
	    struct core_inode *inode;
	    if(dir_blk.entries[i].name[0] == 0)
		continue;
	    inode = make_inode(dir->inode->dev, dir_blk.entries[i].inum);
	    if(inode != NULL)
	    {
		struct time_bits tm;
		expand_time(inode->inode.modtime, &tm);
		SHELL->printf(sh,
			      "%5d %s %2d %8d  %s %2d %02d:%02d  %s",
			      inode->inum,
			      decode_attributes(inode->inode.attr),
			      inode->inode.nlinks,
			      inode->inode.size,
			      tm.month_abbrev,
			      tm.day,
			      tm.hour,
			      tm.minute,
			      dir_blk.entries[i].name);
		if(inode->inode.attr & ATTR_SYMLINK)
		{
		    struct file *link = make_file(inode);
		    if(link != NULL)
		    {
			char buf[inode->inode.size];	/* GCC-specific */
			read_file(buf, inode->inode.size, link);
			SHELL->printf(sh, " -> %s", buf);
			close_file(link);
		    }
		}
		SHELL->printf(sh, "\n");
		close_inode(inode);
	    }
	}
    }
error:
    rc = RC_FAIL;
end:
    close_file(dir);
    return rc;
}

#define DOC_cd "cd DIRECTORY\n\
Change the current working directory to DIRECTORY."
int
cmd_cd(struct shell *sh, int argc, char **argv)
{
    if(argc == 1)
    {
	struct file *dir = open_file(argv[0], F_READ | F_WRITE | F_ALLOW_DIR);
	if(dir != NULL)
	{
	    struct file *old;
	    ERRNO = E_OK;
 	    old = swap_current_dir(dir);
	    if(old != NULL)
		close_file(old);
	    if(ERRNO == E_OK)
		return RC_OK;
	    close_file(dir);
	    SHELL->perror(sh, "cd");
	}
	else
	    SHELL->perror(sh, argv[0]);
	return RC_FAIL;
    }
    else
	return SHELL->arg_error(sh);
}

#define DOC_ln "ln [-s] SOURCE-FILE DEST-FILE\n\
Create a new link called DEST-FILE to the file SOURCE-FILE."
int
cmd_ln(struct shell *sh, int argc, char **argv)
{
    struct file *src;
    int rc = RC_FAIL;
    if((argc == 3) && !strcmp(argv[0], "-s"))
    {
	/* Symbolic link. */
	rc = make_symlink(argv[2], argv[1]) ? RC_OK : RC_FAIL;
	if(rc == RC_FAIL)
	    SHELL->perror(sh, argv[2]);
    }
    else if(argc == 2)
    {
	/* Hard link. */
	src = open_file(argv[0], F_READ | F_WRITE);
	if(src != NULL)
	{
	    if(!make_link(argv[1], src))
		SHELL->perror(sh, "make_link");
	    else
		rc = RC_OK;
	    close_file(src);
	}
	else
	    SHELL->perror(sh, argv[0]);
    }
    else
	rc = SHELL->arg_error(sh);
    return rc;
}

#define DOC_mkdir "mkdir PATH-NAME\n\
Creates a new, empty, directory called PATH-NAME."
int
cmd_mkdir(struct shell *sh, int argc, char **argv)
{
    if(argc != 1)
	return SHELL->arg_error(sh);
    if(!make_directory(argv[0], 0))
    {
	SHELL->perror(sh, argv[0]);
	return RC_FAIL;
    }
    return RC_OK;
}

#define DOC_rm "rm FILES...\n\
Deletes each of the FILES."
int
cmd_rm(struct shell *sh, int argc, char **argv)
{
    int rc = RC_OK;
    while(rc == RC_OK && argc > 0)
    {
	struct file *file = open_file(*argv, F_READ | F_DONT_LINK);
	if(file == NULL)
	{
	    SHELL->perror(sh, *argv);
	    rc = RC_FAIL;
	    break;
	}
	if(F_IS_DIR(file))
	{
	    SHELL->printf(sh, "%s: Can't delete directories\n", *argv);
	    rc = RC_WARN;
	}
	else if(!remove_link(*argv))
	{
	    SHELL->perror(sh, *argv);
	    rc = RC_FAIL;
	}
	close_file(file);
	argc--; argv++;
    }
    return rc;
}

#define DOC_rmdir "rmdir DIRECTORY\n\
If the directory DIRECTORY is empty, delete it."
int
cmd_rmdir(struct shell *sh, int argc, char **argv)
{
    if(argc != 1)
	return SHELL->arg_error(sh);
    if(!remove_directory(*argv))
    {
	SHELL->perror(sh, *argv);
	return RC_FAIL;
    }
    return RC_OK;
}

#define DOC_mv "mv SOURCE-FILE DEST-FILE\n\
Rename the file SOURCE-FILE as DEST-FILE, note that you can't move files\
across devices."
int
cmd_mv(struct shell *sh, int argc, char **argv)
{
    struct file *src;
    if(argc != 2)
	return SHELL->arg_error(sh);
    src = open_file(argv[0], F_READ | F_DONT_LINK | F_ALLOW_DIR);
    if(src == NULL)
    {
	SHELL->perror(sh, argv[0]);
	return RC_FAIL;
    }
    if(make_link(argv[1], src))
    {
	remove_link(argv[0]);
	close_file(src);
	return RC_OK;
    }
    SHELL->perror(sh, argv[1]);
    return RC_OK;
}
	
static inline void
print_devinfo(struct shell *sh, struct fs_device *dev)
{
    SHELL->printf(sh, "%10s  %8d  %8d  %8d\n",
		  dev->name, dev->sup.total_blocks,
		  used_blocks(dev), dev->use_count);
}
#define DOC_devinfo "devinfo [DEVICE-NAME]\n\
If DEVICE-NAME is not specified, prints some information about all\n\
devices installed in the file system. Otherwise just prints information\n\
about the device called DEVICE-NAME."
int
cmd_devinfo(struct shell *sh, int argc, char **argv)
{
    int rc = RC_OK;
    SHELL->printf(sh, "%10s  %8s  %8s  %8s\n",
		  "Device", "Blocks", "Used", "Users");
    if(argc == 1)
    {
	struct fs_device *dev;
	char *tmp = strchr(argv[0], ':');
	if(tmp != NULL)
	    *tmp = 0;
	dev = get_device(argv[0]);
	if(dev != NULL)
	{
	    print_devinfo(sh, dev);
	    release_device(dev);
	}
	else
	{
	    SHELL->printf(sh, "Can't find device, %s:\n", argv[0]);
	    rc = RC_FAIL;
	}
    }
    else
    {
	struct fs_device *x = device_list;
	while(x != NULL)
	{
	    print_devinfo(sh, x);
	    x = x->next;
	}
    }
    return rc;
}

#ifdef TEST
#define DOC_ucp "ucp UNIX-FILE DEST-FILE\n\
Copy the file UNIX-FILE from the Unix file system to the VMM one."
int
cmd_ucp(struct shell *sh, int argc, char **argv)
{
    int rc = RC_FAIL;
    if(argc == 2)
    {
	int src_fd = open(argv[0], O_RDONLY);
	if(src_fd >= 0)
	{
	    struct file *dst = open_file(argv[1],
					 F_WRITE | F_TRUNCATE | F_CREATE);
	    if(dst != NULL)
	    {
		int actual;
		rc = RC_OK;
		do {
		    int wrote;
		    u_char buf[FS_BLKSIZ];
		    actual = read(src_fd, &buf, FS_BLKSIZ);
		    if(actual < 0)
		    {
			perror(argv[0]);
			rc = RC_FAIL;
			break;
		    }
		    wrote = write_file(&buf, actual, dst);
		    if(wrote != actual)
		    {
			SHELL->perror(sh, argv[1]);
			rc = RC_FAIL;
			break;
		    }
		} while(actual > 0);
		close_file(dst);
	    }
	    close(src_fd);
	}
    }
    else
	rc = SHELL->arg_error(sh);
    return rc;
}
#endif /* TEST */

#define DOC_bufstats "bufstats\n\
Display some information about buffer-cache usage."
int
cmd_bufstats(struct shell *sh, __attribute__ ((unused)) int argc,
                 __attribute__ ((unused)) char **argv)
{
    SHELL->printf(sh, "  Total block accesses: %-8d\n"
		  "       Cached accesses: %-8d\n"
		  "Discarded dirty blocks: %-8d\n",
		  total_accessed, cached_accesses, dirty_accesses);
    return RC_OK;
}

#define DOC_mount "mount [-hd PARTITION-NAME]...\n\
Mount a device in the file system."
int
cmd_mount(struct shell *sh, int argc, char **argv)
{
    int rc = 0;
    while(argc >= 2)
    {
	if(!strcmp("-hd", *argv))
	{
	    struct hd_module *hd = (struct hd_module *)kernel->open_module("hd", SYS_VER);
	    if(hd != NULL)
	    {
		hd_partition_t *p = hd->find_partition(argv[1]);
		if(p == NULL)
		{
		    SHELL->printf(sh, "Error: no partition `%s'\n",
				  argv[1]);
		    rc = RC_FAIL;
		}
		if(!hd->mount_partition(p, FALSE))
		    rc = RC_FAIL;
		kernel->close_module((struct module *)hd);
	    }
	    else
		rc = RC_FAIL;
	}
	argc--; argv++;
    }
    return rc;
}

#define DOC_umount "umount DEV-NAME\n\
Unmount the device called DEV-NAME."
int
cmd_umount(struct shell *sh, int argc, char **argv)
{
    int rc = 0;
    struct fs_device *dev;
    if(argc == 0)
	return 0;
    dev = get_device(*argv);
    if(dev == NULL)
	goto error;
    if(!remove_device(dev))
    {
	release_device(dev);
    error:
	SHELL->perror(sh, *argv);
	return RC_FAIL;
    }
    release_device(dev);
    return rc;
}

#define DOC_mkfs "mkfs [-hd PARTITION-NAME [RESERVED-BLOCKS]]\n\
Create a new file system structure on the hard-disk partition called\n\
PARTITION-NAME."
int
cmd_mkfs(struct shell *sh, int argc, char **argv)
{
    int rc = 0;
    if(argc >= 2)
    {
	if(!strcmp("-hd", argv[0]))
	{
	    struct hd_module *hd = (struct hd_module *)kernel->open_module("hd", SYS_VER);
	    if(hd != NULL)
	    {
		hd_partition_t *p;
		u_long reserved = 0;
		p = hd->find_partition(argv[1]);
		if(p == NULL)
		{
		    SHELL->printf(sh, "Error: no partition `%s'\n",
				      argv[1]);
		    rc = RC_FAIL;
		}
		if(argc > 2)
		    reserved = kernel->strtoul(argv[2], NULL, 0);
		if(!hd->mkfs_partition(p, reserved))
		    rc = RC_FAIL;
		kernel->close_module((struct module *)hd);
	    }
	    else
		rc = RC_FAIL;
	}
    }
    return rc;
}

struct shell_cmds fs_cmds =
{
    0,
    { CMD(cp), CMD(type), CMD(ls), CMD(cd), CMD(ln), CMD(mkdir),
      CMD(rm), CMD(rmdir), CMD(mv), CMD(devinfo), CMD(bufstats),
      CMD(mount), CMD(umount), CMD(mkfs),
#ifdef TEST
      CMD(ucp),
#endif
      END_CMD }
};
      
bool
add_fs_commands(void)
{
#ifdef TEST
    SHELL->add_cmd_list(&fs_cmds);
#else
    kernel->add_shell_cmds(&fs_cmds);
#endif
    return TRUE;
}
