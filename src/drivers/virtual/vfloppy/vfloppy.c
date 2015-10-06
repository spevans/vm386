/* vfloppy.c -- Virtual floppy device.

   Simon Evans. */

#define DEBUG
#include <vmm/vfloppy.h>
#include <vmm/kernel.h>
#include <vmm/fs.h>
#include <vmm/vm.h>
#include <vmm/errno.h>
#include <vmm/segment.h>
#include <vmm/string.h>

#define kprintf kernel->printf

struct vfloppy {
    struct vm *vm;
    bool got_disk;
    bool is_file;
    union {
	struct file *file;
    } vdisk;
    size_t blocks;
    size_t sectors_per_track;
    u_short last_stat;
    u_short status, error;
    u_char irq;

    struct vm_kill_handler kh;
};

struct kernel_module *kernel;
struct fs_module *fs;
struct vm_module *vm;

static int vm_slot;



static void
delete_vfloppy(struct vm *vm)
{
    struct vfloppy *v = vm->slots[vm_slot];
    if(v != NULL)
    {
	vm->slots[vm_slot] = NULL;
	if(v->is_file)
	    fs->close(v->vdisk.file);
	kernel->free(v);
	vfloppy_module.base.vxd_base.open_count--;
    }
}

static bool
create_vfloppy(struct vm *vmach, int argc, char **argv)
{
    struct vfloppy *new;
    new = kernel->calloc(sizeof(struct vfloppy), 1);
    if(new != NULL)
    {
	new->vm = vmach;
	{
	    new->is_file = FALSE;
	    new->got_disk = FALSE;
            new->kh.func = delete_vfloppy;
	    vm->add_vm_kill_handler(vmach, &new->kh);
	    vmach->slots[vm_slot] = new;
	    vfloppy_module.base.vxd_base.open_count++;
	    if(argc)
	        change_vfloppy(vmach, *argv);
	    return TRUE;
	}
	kernel->free(new);
    }
    return FALSE;
}


/* Block access (for VBIOS). */

void
kill_vfloppy(struct vm *vm)
{
    struct vfloppy *flop = vm->slots[vm_slot];
    if(flop && flop->vdisk.file != NULL)
    {
	fs->close(flop->vdisk.file);
	flop->vdisk.file = NULL;
    }
}

bool
change_vfloppy(struct vm *vm, const char *new_file)
{
    struct vfloppy *flop = vm->slots[vm_slot];
    struct file *f = fs->open(new_file, F_READ);

    if(flop == NULL) return FALSE;
    if(f != NULL)
    {
	if(flop->vdisk.file != NULL)
	    fs->close(flop->vdisk.file);
	flop->vdisk.file = f;
	flop->sectors_per_track = F_SIZE(flop->vdisk.file) / (512 * 160);
	flop->last_stat = 0;
	flop->got_disk = TRUE;
	flop->is_file = TRUE;
	return TRUE;
    }
    flop->got_disk = FALSE;
    return FALSE;
}

int
vfloppy_read_sectors(struct vm *vm, u_int drvno, u_int head, u_int track,
                            u_int sector, int count, void *buf)

{
    struct vfloppy *flop = vm->slots[vm_slot];
    u_long blkno;
    int i;
    if(flop == NULL) return 0;
    DB(("vfd_read: drvno=%u buf=%p head=%d track=%d sector=%d count=%d\n",
	drvno, buf, head, track, sector, count));
    sector--;
    if(flop->got_disk == FALSE)
    {
	DB(("vfd_read: No image file!\n"));
	flop->last_stat = 0x80;
	return 0;
    }
    if((head >= 2) || (track >= 80) || (sector >= flop->sectors_per_track))
    {
	DB(("vfd_read: Arguments out of range!\n"));
	flop->last_stat = 0x04;
	return 0;
    }
    blkno = sector + (head * flop->sectors_per_track)
	    + (track * 2 * flop->sectors_per_track);
    if(fs->seek(flop->vdisk.file, blkno * 512, SEEK_ABS) < 0)
    {
	DB(("vfd_read: Can't seek to block %u\n", blkno));
	flop->last_stat = 0x04;
	return 0;
    }
    for(i = 0; i < count; i++)
    {
	char tmp_buf[512];
	if(fs->read(tmp_buf, 512, flop->vdisk.file) != 512)
	{
	    DB(("vfd_read: Can't read block.\n"));
	    flop->last_stat = 0x0c;
	    return i;
	}
	memcpy_to_user(buf, tmp_buf, 512);
	buf += 512;
    }
    DB(("vfd_read: Read %d blocks\n", i));
    flop->last_stat = 0;
    return i;
}


bool
vfloppy_get_status(struct vm *vm, u_char *statp, u_char *errp)
{
    struct vfloppy *v = vm->slots[vm_slot];
    if(v == NULL)
        return FALSE;
    *statp = v->status;
    *errp = v->error;
    return TRUE;
}


/* Module stuff. */

static bool
vfloppy_init(void)
{
    fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
    if(fs != NULL)
    {
        vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
	if(vm != NULL)
	{
	    vm_slot = vm->alloc_vm_slot();
	    if(vm_slot >= 0)
	    {
		kernel->add_shell_cmds(&vfloppy_cmds);
		return TRUE;
	    }
	    kernel->close_module((struct module *)vm);
	}
	kernel->close_module((struct module *)fs);
    }
    return FALSE;
}

static bool
vfloppy_expunge(void)
{
    DB(("Attempting to expunge vfloppy..."));
    if(vfloppy_module.base.vxd_base.open_count == 0)
    {
	vm->free_vm_slot(vm_slot);
	kernel->remove_shell_cmds(&vfloppy_cmds);
	kernel->close_module((struct module *)vm);
	kernel->close_module((struct module *)fs);
	return TRUE;
        DB(("done\n"));
    }
    DB(("not done\n"));
    return FALSE;
}

struct vfloppy_module vfloppy_module =
{
    { MODULE_INIT("vfloppy", SYS_VER, vfloppy_init, NULL, NULL, vfloppy_expunge),
     create_vfloppy },
    delete_vfloppy, vfloppy_read_sectors, vfloppy_get_status
};
