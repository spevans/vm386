/* fs.h -- File system definitions.
   John Harper. */

#ifndef _VMM_FS_H
#define _VMM_FS_H

#include <vmm/types.h>
#include <vmm/time.h>
#include <vmm/module.h>
#include <vmm/lists.h>
#include <vmm/tasks.h>
#include <stdarg.h>

#ifdef TEST
# include <vmm/shell.h>
#endif

// MBR partition type flag
#define VMM_PARTITION_TYPE 0x30

// MBR offset to partition table
#define PART_TABLE_OFFSET 0x1BE

/* Perhaps it would be better to use 1024-byte blocks.. The value 512 isn't
   hardcoded anywhere so it would be easy to switch.. (It has to be some
   power of 2 though.) */
#define FS_BLKSIZ 1024


/* The boot block is the first on the device (surprise, surprise..) */
#define BOOT_BLK 0

typedef u_long blkno;
typedef u_char blk[FS_BLKSIZ];

/* Returned from functions that allocate blocks if none are free */
#define NO_FREE_BLOCK 0xffffffff

/* The layout of a file system is something like:

	+------------------+
	| Boot/super block |
	+------------------+
	|   Inode bitmap   |
	+- - - - - - - - - +
	|   Inode blocks   |
	+ - - - - - - - - -+
	|   Data bitmap    |
	+- - - - - - - - - +
	|   Data blocks    |  */

struct super_data {
    u_long total_blocks;
    blkno inode_bitmap;
    blkno inodes;
    u_long num_inodes;
    blkno data_bitmap;
    blkno data;
    u_long data_size;		/* Number of data blocks, size of bitmap. */
};

struct boot_blk {
    char boot_code[512 - 4 - sizeof(struct super_data)];
    struct super_data sup;
    u_long magic;
    char reserved[FS_BLKSIZ - 512];
};
#define FS_BOOT_MAGIC   0xAA554d57
#define FS_NOBOOT_MAGIC 0x00004d57

/* An inode as stored on disk. */
struct inode {
    u_long attr;
    size_t size;
    time32_t modtime;
    u_long nlinks;
    /* Pointers to data blocks. data[9->11] are single, double and
       triple indirect blocks. */
    u_long data[12];
};

#define ATTR_NO_READ	0x00000001
#define ATTR_NO_WRITE	0x00000002
#define ATTR_EXEC	0x00000004
#define ATTR_MODE_MASK	0x000000ff
#define ATTR_DIRECTORY	0x00010000
#define ATTR_SYMLINK	0x00020000

#define SINGLE_INDIRECT 9
#define DOUBLE_INDIRECT 10
#define TRIPLE_INDIRECT 11
#define INODES_PER_BLOCK (FS_BLKSIZ / sizeof(struct inode))
#define PTRS_PER_INDIRECT (FS_BLKSIZ / sizeof(blkno))

#define ROOT_INUM 0

struct inode_blk {
    struct inode inodes[INODES_PER_BLOCK];
    char pad[FS_BLKSIZ - (INODES_PER_BLOCK * sizeof(struct inode))];
};

struct indirect_blk {
    blkno data[PTRS_PER_INDIRECT];
};

/* An inode as stored in memory. */
struct core_inode {
    struct core_inode *next;
    int use_count;
    struct fs_device *dev;
    struct inode inode;
    u_long inum;
    bool dirty;
    bool invalid;
#ifndef TEST
    bool locked;
    struct task_list *locked_tasks;
#endif
};
#define NR_INODES 30

/* A file handle. */
struct file {
    struct file *next;
    struct core_inode *inode;
    u_long mode;
    u_long pos;
};
#define NR_FILES 50

/* Modes for opening files. */
#define F_READ		1	/* Open for reading. */
#define F_WRITE		2	/* Open for writing. */
#define F_CREATE	4	/* Create the file if it doesn't exist. */
#define F_TRUNCATE	8	/* Truncate the file to zero bytes. */
#define F_ALLOW_DIR	16	/* Allow the opening of directories. */
#define F_DONT_LINK	32	/* Don't follow symlinks. */

/* Operations on file handles. */
#define F_ATTR(f)	((f)->inode->inode.attr)
#define F_NLINKS(f)	((f)->inode->inode.nlinks)
#define F_SIZE(f)	((f)->inode->inode.size)
#define F_MODTIME(f)	((f)->inode->inode.modtime)
#define F_INUM(f)	((f)->inode->inum)
#define F_IS_DIR(f)	(F_ATTR(f) & ATTR_DIRECTORY)
#define F_IS_SYMLINK(f)	(F_ATTR(f) & ATTR_SYMLINK)
#define F_IS_REG(f)	(!F_IS_DIR(f) && !F_IS_SYMLINK(f))
#define F_READABLE(f)	(!(F_ATTR(f) & ATTR_NO_READ))
#define F_WRITEABLE(f)	(!(F_ATTR(f) & ATTR_NO_WRITE))
#define F_EXECABLE(f)	((F_ATTR(f) & ATTR_EXEC))

/* TYPE arguments to seek_file(). */
#define SEEK_ABS	0	/* N bytes from the start of the file. */
#define SEEK_REL	1	/* N bytes from the current position. */
#define SEEK_EOF	2	/* N bytes back from the end of the file. */

#ifndef EOF
# define EOF (-1)
#endif

#define NAME_MAX 27
struct dir_entry {
    char name[NAME_MAX + 1];	/* a null name means a free slot. */
    u_long inum;
};
#define DIR_ENTRIES_PER_BLOCK (FS_BLKSIZ / sizeof(struct dir_entry))

struct dir_entry_blk {
    struct dir_entry entries[DIR_ENTRIES_PER_BLOCK];
    char pad[FS_BLKSIZ - (DIR_ENTRIES_PER_BLOCK * sizeof(struct dir_entry))];
};


/* One buffer in the buffer cache. Currently a limited number of buffers
   are allocated statically, none dynamically. */
struct buf_head {
    union {
	list_node_t node;
	struct buf_head *next_free;
    } link;
    struct fs_device *dev;
    blkno blkno;
    short use_count;
    bool dirty;
    bool invalid;
#ifndef TEST
    bool locked;
    struct task_list *locked_tasks;
#endif
    union {
	blk data;
	struct boot_blk boot;
	struct inode_blk inodes;
	struct dir_entry_blk dir;
	struct indirect_blk ind;
	u_long bmap[FS_BLKSIZ / 4];
    } buf;
};

#define NR_BUFFERS 20
#define BUFFER_BUCKETS 4
#define BUFFER_HASH(blkno) ((blkno) % BUFFER_BUCKETS)


/* A device which the file system can access, there's a list of these
   somewhere. NAME is the device identifier. READ-BLOCK and WRITE-BLOCK
   are used to access the device. TEST-MEDIA is needed by devices with
   removable media.  */
struct fs_device {
    /* The name of the device, used in path names (i.e. `DEV:foo/bar') */
    const char *name;

    /* These return >=0 on success, or a negative E_?? value on error. If
       no disk is present they should return -E_NODISK.
         These functions may put the current task to sleep if they
       want to.
         Note that BLOCK and COUNT are in terms of FS_BLKSIZ sized blocks. */
    long (*read_blocks)(void *user_data, blkno block, void *buf, int count);
    long (*write_blocks)(void *user_data, blkno block, void *buf, int count);

    /* Devices with removable media (i.e. floppys) should define this
       function, when called it returns E_NODISK if no disk is in the
       drive, E_DISKCHANGE if a new disk was inserted since it was
       last called, or 0 if nothing changed. Note that this function
       may *not* sleep. */
    long (*test_media)(void *user_data);

    /* Each time one of the above functions is called the following
       pointer is passed to it as it's first argument. This allows
       device driver functions to receive a reference to some internal
       data structure. */
    void *user_data;

    /* All of the following fields are initialised by the fs. */
    struct fs_device *next;
    struct super_data sup;	/* read from the boot block */
    struct core_inode *root;	/* may be NULL if device is invalid */
    int use_count;		/* number of live references */
    bool read_only;		/* TRUE for write-protected devices */
    bool invalid;		/* TRUE when device is invalid */
};

#define NR_DEVICES 20

#define FS_READ_BLOCKS(dev, blk, buf, count) \
    ((dev)->read_blocks((dev)->user_data, blk, buf, count))

#define FS_WRITE_BLOCKS(dev, blk, buf, count) \
    ((dev)->write_blocks((dev)->user_data, blk, buf, count))


struct fs_module {
    struct module base;

    /* Device handling */
    struct fs_device *(*alloc_device)(void);
    bool (*add_device)(struct fs_device *dev);
    bool (*remove_device)(struct fs_device *dev);
    struct fs_device *(*get_device)(const char *name);
    void (*release_device)(struct fs_device *dev);

    /* File handling */
    struct file *(*create)(const char *name, u_long attr);
    struct file *(*open)(const char *name, u_long mode);
    void (*close)(struct file *f);
    long (*read)(void *buf, size_t len, struct file *f);
    long (*write)(const void *buf, size_t len, struct file *f);
    long (*seek)(struct file *f, u_long arg, int type);
    struct file *(*dup)(struct file *f);
    bool (*truncate)(struct file *f);
    bool (*set_file_size)(struct file *f, size_t size);
    bool (*make_link)(const char *name, struct file *src);
    bool (*remove_link)(const char *name);
    bool (*set_file_mode)(const char *name, u_long modes);
    bool (*make_directory)(const char *name, u_long attr);
    bool (*remove_directory)(const char *name);
    struct file *(*get_current_dir)(void);
    struct file *(*swap_current_dir)(struct file *dir);
    bool (*make_symlink)(const char *name, const char *link);
    bool (*mkfs)(struct fs_device *dev, u_long blocks, u_long reserved);

    /* Buffer-cache functions. */
    struct buf_head *(*bread)(struct fs_device *dev, blkno blk);
    bool (*bwrite)(struct fs_device *dev, blkno blk, const void *data);
    void (*bdirty)(struct buf_head *bh, bool write_now);
    void (*brelse)(struct buf_head *bh);

    /* Library functions. */
    int (*putc)(u_char c, struct file *fh);
    int (*getc)(struct file *fh);
    char *(*read_line)(char *buf, size_t bufsiz, struct file *fh);
    int (*write_string)(const char *str, struct file *fh);
    int (*fvprintf)(struct file *fh, const char *fmt, va_list args);
    int (*fprintf)(struct file *fh, const char *fmt, ...);
};

#ifndef TEST
# define CURRENT_DIR (kernel->current_task->current_dir)
# define FORBID() forbid()
# define PERMIT() permit()
#else
# define CURRENT_DIR current_dir
extern struct file *current_dir;
# define FORBID() do {} while(0)
# define PERMIT() do {} while(0)
#endif


/* Function prototypes. */

#ifdef FS_MODULE

/* from dev.c */
extern struct fs_device *device_list;
extern void init_devices(void);
extern struct fs_device *alloc_device(void);
extern bool add_device(struct fs_device *dev);
extern struct fs_device *get_device(const char *name);
extern void release_device(struct fs_device *dev);
extern bool remove_device(struct fs_device *dev);
extern bool read_meta_data(struct fs_device *dev);
extern void invalidate_device(struct fs_device *dev);
extern bool validate_device(struct fs_device *dev);

/* from bitmap.c */
extern blkno bmap_alloc(struct fs_device *dev, blkno bmap_start, u_long bmap_len);
extern bool bmap_free(struct fs_device *dev, blkno bmap_start, u_long bit);
extern blkno alloc_block(struct fs_device *dev, blkno locality);
extern bool free_block(struct fs_device *dev, blkno blk);
extern u_long used_blocks(struct fs_device *dev);

/* from inode.c */
extern void init_inodes(void);
extern void kill_inodes(void);
extern struct core_inode *make_inode(struct fs_device *dev, u_long inum);
extern struct core_inode *dup_inode(struct core_inode *inode);
extern void close_inode(struct core_inode *inode);
extern bool invalidate_device_inodes(struct fs_device *dev);
extern long alloc_inode(struct fs_device *dev);
extern bool free_inode(struct fs_device *dev, u_long inum);
extern bool read_inode(struct core_inode *inode);
extern bool write_inode(struct core_inode *inode);
extern blkno get_data_blkno(struct core_inode *inode, blkno blk, bool create);
extern struct buf_head *get_data_block(struct core_inode *inode, blkno blk, bool create);

/* from fs_mod.c */
extern struct fs_module fs_module;
extern bool fs_init(void);
extern void fs_kill(void);

/* from fs_cmds.c */
extern bool add_fs_commands(void);

/* from file.c */
extern void init_files(void);
extern void kill_files(void);
extern struct file *make_file(struct core_inode *inode);
extern void close_file(struct file *file);
extern struct file *dup_file(struct file *file);
extern long seek_file(struct file *file, u_long arg, int type);
extern long read_file(void *buf, size_t len, struct file *file);
extern long write_file(const void *buf, size_t len, struct file *file);
extern bool delete_inode_data(struct core_inode *inode);
extern bool truncate_file(struct file *file);
extern bool set_file_size(struct file *file, size_t size);
extern bool set_file_modes(const char *name, u_long mode);

/* from dir.c */
extern struct file *current_dir;
extern bool file_exists_p(struct file *dir, const char *name);
extern bool directory_empty_p(struct file *dir);
extern struct file *read_symlink(struct file *link);
extern struct file *open_file(const char *name, u_long mode);
extern bool make_link(const char *name, struct file *src);
extern struct file *create_file(const char *name, u_long attr);
extern bool make_directory(const char *name, u_long attr);
extern bool make_symlink(const char *name, const char *symlink);
extern bool remove_link(const char *name);
extern bool remove_directory(const char *name);
extern struct file *get_current_dir(void);
extern struct file *swap_current_dir(struct file *dir);

/* from buffer.c */
extern u_long total_accessed, cached_accesses, dirty_accesses;
extern void init_buffers(void);
extern void kill_buffers(void);
extern struct buf_head *bread(struct fs_device *dev, blkno blk);
extern bool bwrite(struct fs_device *dev, blkno blk, const void *data);
extern void bdirty(struct buf_head *bh, bool write_now);
extern void brelse(struct buf_head *bh);
extern void flush_device_cache(struct fs_device *dev, bool dont_write);
extern bool test_media(struct fs_device *dev);

/* from mkfs.c */
extern bool mkfs(struct fs_device *dev, u_long blocks, u_long reserved);

/* from lib.c */
extern int fs_putc(u_char c, struct file *fh);
extern int fs_getc(struct file *fh);
extern char *fs_read_line(char *buf, size_t bufsiz, struct file *fh);
extern int fs_write_string(const char *str, struct file *fh);
extern int fs_fvprintf(struct file *fh, const char *fmt, va_list args);
extern int fs_fprintf(struct file *fh, const char *fmt, ...);

#ifdef TEST
  /* from test_dev.c */
  extern bool open_test_dev(const char *file, bool mkfs, u_long reserved);
  extern void close_test_dev(void);

  /* from ../shell/test.c */
  extern struct shell_module *shell;
#endif

#endif /* FS_MODULE */
#endif /* _VMM_FS_H */
