/* dir.c -- Directory handling.
   John Harper. */

#include <vmm/fs.h>
#include <vmm/errno.h>
#include <vmm/string.h>
#include <vmm/kernel.h>
#include <vmm/hd.h>

#ifndef TEST
# define kprintf kernel->printf
# define current_time kernel->current_time
#else
struct file *current_dir;
#endif

extern char root_dir[];

static struct dir_entry_blk tmp_dir_blk;

/* Maximum number of symlinks we can recurse through in one go (to stop
   us smashing the stack). */
#define MAX_SYMLINK_DEPTH 8

/* Return an inode pointing at the file called NAME in the directory
   DIR, or NULL if no such file exists.
   Note that the position of DIR on exiting this function is undefined. */
static struct core_inode *
find_file_entry(struct file *dir, const char *name)
{
    long tmp;
    size_t dir_len = dir->inode->inode.size;
    if(!F_IS_DIR(dir))
    {
	ERRNO = E_NOTDIR;
	return NULL;
    }
    seek_file(dir, 0, SEEK_ABS);
    while(dir_len > 0)
    {
	int i;
	tmp = read_file(&tmp_dir_blk, max(dir_len, FS_BLKSIZ), dir);
	if(tmp < 0)
	    return NULL;
	for(i = 0; i < (tmp / sizeof(struct dir_entry)); i++)
	{
	    if((tmp_dir_blk.entries[i].name[0] != 0)
	       && !strcmp(name, tmp_dir_blk.entries[i].name))
	    {
		return make_inode(dir->inode->dev,
				  tmp_dir_blk.entries[i].inum);
	    }
	}
	dir_len -= tmp;
    }
    ERRNO = E_NOEXIST;
    return NULL;
}

/* Create a new directory entry for the file called NAME in the directory
   DIR. It will point to the inode INUM.
   Note that the position of DIR on exiting this function is undefined. */
static bool
create_file_entry(struct file *dir, const char *name, long inum)
{
    long free_space = -1;
    long tmp;
    size_t dir_len = dir->inode->inode.size;
    struct dir_entry tmp_entry;
    if(!F_IS_DIR(dir))
    {
	ERRNO = E_NOTDIR;
	return FALSE;
    }
    seek_file(dir, 0, SEEK_ABS);
    while(dir_len > 0)
    {
	int i;
	tmp = read_file(&tmp_dir_blk, max(dir_len, FS_BLKSIZ), dir);
	if(tmp < 0)
	    return tmp;
	for(i = 0; i < (tmp / sizeof(struct dir_entry)); i++)
	{
	    if(tmp_dir_blk.entries[i].name[0] == 0)
		free_space = dir->pos - tmp + (i * sizeof(struct dir_entry));
	    if(!strcmp(name, tmp_dir_blk.entries[i].name))
	    {
		ERRNO = E_EXISTS;
		return FALSE;
	    }
	}
	dir_len -= tmp;
    }
    if(free_space != -1)
	seek_file(dir, free_space, SEEK_ABS);
    else
	seek_file(dir, dir->inode->inode.size, SEEK_ABS);
    strncpy(tmp_entry.name, name, NAME_MAX);
    tmp_entry.inum = inum;
    if(write_file(&tmp_entry, sizeof(tmp_entry), dir) < 0)
	return FALSE;
    return TRUE;
}

/* Remove the directory entry for the file called NAME in the directory
   DIR. Returns the inumber of the deleted entry or a negative number
   signalling an error.
   Note that the position of DIR on exiting this function is undefined. */
static long
delete_file_entry(struct file *dir, const char *name)
{
    long tmp;
    size_t dir_len = dir->inode->inode.size;
    if(!F_IS_DIR(dir))
    {
	ERRNO = E_NOTDIR;
	return -1;
    }
    seek_file(dir, 0, SEEK_ABS);
    while(dir_len > 0)
    {
	int i;
	tmp = read_file(&tmp_dir_blk, max(dir_len, FS_BLKSIZ), dir);
	if(tmp < 0)
	    return tmp;
	for(i = 0; i < (tmp / sizeof(struct dir_entry)); i++)
	{
	    if((tmp_dir_blk.entries[i].name[0] != 0)
	       && !strcmp(name, tmp_dir_blk.entries[i].name))
	    {
		long inum = tmp_dir_blk.entries[i].inum;
		seek_file(dir, -(tmp - (i * sizeof(struct dir_entry))), SEEK_REL);
		tmp_dir_blk.entries[i].name[0] = 0;
		tmp_dir_blk.entries[i].inum = 0;
		if(write_file(&tmp_dir_blk.entries[i], sizeof(struct dir_entry), dir) < 0)
		    return -1;
		return inum;
	    }
	}
	dir_len -= tmp;
    }
    ERRNO = E_NOEXIST;
    return -1;
}

/* Returns a file pointing to the base directory of the file specification
   *NAMEP. On exit *NAMEP is set to point at the beginning of the filename
   component. */
static struct file *
find_parent(const char **namep)
{
    const char *name = *namep;
    struct file *dir;
    char buf[40];
    const char *tmp;
    if(*name == '/')
    {
	dir = open_file(root_dir, F_READ | F_WRITE | F_ALLOW_DIR);
	name++;
    }
    else
    {
	tmp = strchr(name, ':');
	if(tmp != NULL)
	{
	    if(tmp == name)
	    {
		/* null device name, root of the current device. */
		if(CURRENT_DIR && CURRENT_DIR->inode->dev->root)
		    dir = make_file(CURRENT_DIR->inode->dev->root);
		else
		{
		    dir = NULL;
		    ERRNO = E_INVALID;
		}
	    }
	    else
	    {
		struct fs_device *dev;
		memcpy(buf, name, tmp - name);
		buf[tmp - name] = 0;
		dev = get_device(buf);
		if(dev == NULL)
		{
		    ERRNO = E_NODEV;
		    return NULL;
		}
		if(dev->root == NULL)
		{
		    ERRNO = E_INVALID;
		    release_device(dev);
		    return NULL;
		}
		dir = make_file(dev->root);
		release_device(dev);
	    }
	    name = tmp + 1;
	}
	else
	    dir = dup_file(CURRENT_DIR);
    }
    if(dir == NULL)
	return NULL;
    while(dir && F_IS_DIR(dir))
    {
	struct core_inode *child;
	tmp = strchr(name, '/');
	if(tmp == NULL)
	    break;
	memcpy(buf, name, tmp - name);
	buf[tmp - name] = 0;
	child = find_file_entry(dir, buf);
	close_file(dir);
	if(child == NULL)
	    return NULL;
	if(!(child->inode.attr & ATTR_SYMLINK))
	{
	    dir = make_file(child);
	    close_inode(child);
	}
	else
	{
	    struct file *child_fh = make_file(child);
	    struct file *link;
	    close_inode(child);
	    if(child_fh == NULL)
		return NULL;
	    child_fh->mode |= F_ALLOW_DIR;
	    link = read_symlink(child_fh);
	    close_file(child_fh);
	    dir = link;
	}
	name = tmp + 1;
    }
    *namep = name;
    return dir;
}


/* Returns TRUE if the directory DIR contains no links other than "." and
   ".." */
bool
directory_empty_p(struct file *dir)
{
    long tmp;
    size_t dir_len = dir->inode->inode.size;
    if(!test_media(dir->inode->dev))
	return FALSE;
    if(!F_IS_DIR(dir))
    {
	ERRNO = E_NOTDIR;
	return FALSE;
    }
    seek_file(dir, 0, SEEK_ABS);
    while(dir_len > 0)
    {
	int i;
	tmp = read_file(&tmp_dir_blk, max(dir_len, FS_BLKSIZ), dir);
	if(tmp < 0)
	    return FALSE;
	for(i = 0; i < (tmp / sizeof(struct dir_entry)); i++)
	{
	    if((tmp_dir_blk.entries[i].name[0] != 0)
	       && strcmp(".", tmp_dir_blk.entries[i].name)
	       && strcmp("..", tmp_dir_blk.entries[i].name))
	    {
		return FALSE;
	    }
	}
	dir_len -= tmp;
    }
    return TRUE;
}

/* Returns TRUE or FALSE depending on whether the file exists. If DIR is
   non-NULL it points to the directory to search and NAME is the name of
   the file in that directory to search for. Otherwise NAME specifies the
   full path of the file. */
bool
file_exists_p(struct file *dir, const char *name)
{
    bool rc, free_dir;
    struct core_inode *inode;
    if(dir == NULL)
    {
	dir = find_parent(&name);
	if(dir == NULL)
	    return FALSE;
	free_dir = TRUE;
    }
    else
	free_dir = FALSE;
    inode = find_file_entry(dir, name);
    if(inode != NULL)
    {
	close_inode(inode);
	rc = TRUE;
    }
    else
	rc = FALSE;
    if(free_dir)
	close_file(dir);
    return rc;
}

struct file *
read_symlink(struct file *link)
{
    struct file *dest = NULL;
    static int symlink_nesting;
    if(++symlink_nesting <= MAX_SYMLINK_DEPTH)
    {
	if(link->inode->inode.attr & ATTR_SYMLINK)
	{
	    if(link->inode->inode.size < 256)
	    {
		char buf[256];
		if(read_file(buf, link->inode->inode.size, link) > 0)
		    dest = open_file(buf, link->mode);
	    }
	    else
		ERRNO = E_NOMEM;
	}
	else
	    ERRNO = E_NOTLINK;
    }
    else
	ERRNO = E_MAXLINKS;
    symlink_nesting--;
    return dest;
}

/* Opens and returns a new file called NAME. MODE is a set of flag bits
   defining how to open it and how it will be used. */
struct file *
open_file(const char *name, u_long mode)
{
    struct file *dir;
    dir = find_parent(&name);
    if(dir && *name)
    {
	struct core_inode *inode = find_file_entry(dir, name);
	struct file *file = NULL;
	if(inode == NULL)
	{
	    if(mode & F_CREATE)
	    {
		struct file *old_dir = swap_current_dir(dir);
		file = create_file(name, 0);
		swap_current_dir(old_dir);
	    }
	}
	else
	{
	    /* Check for a symbolic link first. */
	    if(((mode & F_DONT_LINK) == 0)
		&& (inode->inode.attr & ATTR_SYMLINK))
	    {
		struct file *link = make_file(inode);
		if(link != NULL)
		{
		    link->mode = F_READ | mode;
		    file = read_symlink(link);
		    close_file(link);
		}
	    }
	    else if(((mode & F_READ) && (inode->inode.attr & ATTR_NO_READ))
	       || ((mode & F_WRITE) && (inode->inode.attr & ATTR_NO_WRITE)))
	    {
		ERRNO = E_PERM;
	    }
	    else if((inode->inode.attr & ATTR_DIRECTORY)
		    && !(mode & F_ALLOW_DIR))
	    {
		ERRNO = E_ISDIR;
	    }
	    else
		file = make_file(inode);
	    close_inode(inode);
	}
	if(file != NULL)
	{
	    if(mode & F_TRUNCATE)
		truncate_file(file);
	    file->mode = mode;
	    write_inode(file->inode);
	}
	close_file(dir);
	return file;
    }
    return dir;
}

/* Make a new link to the file SRC, the link's name will be NAME. */
bool
make_link(const char *name, struct file *src)
{
    struct file *dir;
    bool rc = FALSE;
    if(src == NULL)
    {
	ERRNO = E_BADARG;
	return FALSE;
    }
    if(!test_media(src->inode->dev))
	return FALSE;
    dir = find_parent(&name);
    if(dir && *name)
    {
	if(dir->inode->dev != src->inode->dev)
	    ERRNO = E_XDEV;
	else
	{
	    rc = create_file_entry(dir, name, src->inode->inum);
	    close_file(dir);
	    if(rc)
	    {
		src->inode->inode.nlinks++;
		src->inode->dirty = TRUE;
		write_inode(src->inode);
	    }
	}
    }
    return rc;
}

/* Create a new file, called NAME and with attributes ATTR. Returns NULL
   for an error. */
struct file *
create_file(const char *name, u_long attr)
{
    struct file *file = NULL, *dir = find_parent(&name);
    if(dir && *name)
    {
	if(file_exists_p(dir, name))
	    ERRNO = E_EXISTS;
	else
	{
	    long inum = alloc_inode(dir->inode->dev);
	    if(inum >= 0)
	    {
		struct core_inode *tmp = make_inode(dir->inode->dev, inum);
		if(tmp != NULL)
		{
		    memset(&tmp->inode, 0, sizeof(struct inode));
		    tmp->inode.modtime = current_time();
		    tmp->dirty = TRUE;
		    file = make_file(tmp);
		    close_inode(tmp);
		    if(file != NULL)
		    {
			if(create_file_entry(dir, name, inum))
			{
			    file->inode->inode.nlinks++;
			    file->inode->inode.attr = attr;
			    file->inode->dirty = TRUE;
			    write_inode(file->inode);
			}
			else
			    close_file(file);
		    }
		}
		if(!file)
		    free_inode(dir->inode->dev, inum);
	    }
	}
    }
    if(dir != NULL)
	close_file(dir);
    return file;
}

/* Creates a new directory called NAME with attributes ATTR. Also creates
   two links "." and ".." in it. */
bool
make_directory(const char *name, u_long attr)
{
    struct file *new_dir = create_file(name, attr | ATTR_DIRECTORY);
    if(new_dir != NULL)
    {
	struct file *parent = find_parent(&name);
	struct file *old_dir = swap_current_dir(new_dir);
	if(parent != NULL)
	{
	    make_link(".", CURRENT_DIR);
	    make_link("..", parent);
	    close_file(parent);
	}
	swap_current_dir(old_dir);
	close_file(new_dir);
	return TRUE;
    }
    return FALSE;
}

/* Creates a symbolic link called NAME, pointing to the file/dir called
   LINK. */
bool
make_symlink(const char *name, const char *link)
{
    struct file *new = create_file(name, ATTR_SYMLINK);
    if(new != NULL)
    {
	write_file(link, strlen(link) + 1, new);
	close_file(new);
	return TRUE;
    }
    return FALSE;
}

/* Removes the file called NAME. It's probably not a good idea to call
   this on directories (it's likely to result in stranded data). */
bool
remove_link(const char *name)
{
    bool rc = FALSE;
    struct file *dir = find_parent(&name);
    if(dir && *name)
    {
	long inum = delete_file_entry(dir, name);
	if(inum >= 0)
	{
	    struct core_inode *inode = make_inode(dir->inode->dev, inum);
	    if(inode != NULL)
	    {
		--inode->inode.nlinks;
		inode->dirty = TRUE;
		close_inode(inode);
		rc = TRUE;
	    }
	}
	close_file(dir);
    }
    return rc;
}

/* If the directory NAME is empty (no links but "." and "..") delete it
   and return TRUE. */
bool
remove_directory(const char *name)
{
    bool rc = FALSE;
    struct file *parent = find_parent(&name);
    if(parent && *name)
    {
	long inum;
	struct file *old_dir = swap_current_dir(parent);
	struct file *dir = open_file(name, F_READ | F_ALLOW_DIR);
	if(dir != NULL)
	{
	    if(directory_empty_p(dir))
	    {
		swap_current_dir(dir);
		remove_link(".");
		remove_link("..");
		swap_current_dir(parent);
		inum = delete_file_entry(parent, name);
		if(inum >= 0)
		{
		    --dir->inode->inode.nlinks;
		    dir->inode->dirty = TRUE;
		    rc = TRUE;
		}
	    }
	    else
		ERRNO = E_EXISTS;
	    close_file(dir);
	    swap_current_dir(old_dir);
	}
	close_file(parent);
    }
    return rc;
}

/* Returns a file opened on the current directory. */
struct file *
get_current_dir(void)
{
    if(CURRENT_DIR)
    {
	if(!test_media(CURRENT_DIR->inode->dev))
	    return NULL;
	return dup_file(CURRENT_DIR);
    }
    ERRNO = E_INVALID;
    return NULL;
}

/* Changes the current directory to DIR and returns the old current
   dir. Note that DIR may be NULL and this function may return NULL,
   this simply means that there is no current directory. */
struct file *
swap_current_dir(struct file *dir)
{
    struct file *tmp = CURRENT_DIR;
    if(dir != NULL)
    {
	if(!test_media(dir->inode->dev))
	    return NULL;
	if(!F_IS_DIR(dir))
	{
	    ERRNO = E_NOTDIR;
	    return NULL;
	}
    }
    CURRENT_DIR = dir;
    return tmp;
}
