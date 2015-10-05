/* vide.c -- Virtual IDE device.

   Currently this only supports a single disk on the controller, also it's
   totally untested..

   John Harper. */

#include <vmm/vide.h>
#include <vmm/hd.h>
#include <vmm/hdreg.h>
#include <vmm/kernel.h>
#include <vmm/fs.h>
#include <vmm/vm.h>
#include <vmm/vpic.h>
#include <vmm/errno.h>
#include <vmm/segment.h>
#include <vmm/string.h>
#include <vmm/vcmos.h>

#define kprintf kernel->printf

struct vide {
    struct vm *vm;
    bool is_file;
    union {
	struct file *file;
	hd_partition_t *disk;
    } vdisk;
    size_t blocks;
    size_t cylinders, heads, sectors;
    u_char irq;

    /* Controller registers. */
    u_char error, features, num_sectors, sector, low_cyl, high_cyl;
    u_char select, status, command;
    u_char devctrl;

    bool intrq;

    u_long buf_index, block, blocks_left;
    u_char buf[512];

    struct vm_kill_handler kh;
};

struct kernel_module *kernel;
struct fs_module *fs;
struct vm_module *vm;
struct hd_module *hd;
struct vpic_module *vpic;

static int vm_slot;

#define RDY_STAT	(HD_STAT_DRDY | HD_STAT_DSC)
#define DATA_RDY_STAT	(RDY_STAT | HD_STAT_DRDY)
#define ERR_STAT	HD_STAT_ERR
#define WERR_STAT	(HD_STAT_ERR | HD_STAT_DWF)
#define BUSY_STAT	HD_STAT_BSY



static void
delete_vide(struct vm *vm)
{
    struct vide *v = vm->slots[vm_slot];
    if(v != NULL)
    {
	vm->slots[vm_slot] = NULL;
	if(v->is_file)
	    fs->close(v->vdisk.file);
	kernel->free(v);
	vide_module.base.vxd_base.open_count--;
    }
}

/* Args are `IMAGE-FILE [TOTAL-BLOCKS]' */
static bool
create_vide(struct vm *vmach, int argc, char **argv)
{
    struct vide *new;
    if(argc < 1)
    {
	kprintf("create_vide: Incorrect number of arguments\n"
		"             Usage: vm-vxd vide FILE [BLOCKS]\n");
	return FALSE;
    }
    new = kernel->calloc(sizeof(struct vide), 1);
    if(new != NULL)
    {
	u_long namelen = strlen(argv[0]);
	new->vm = vmach;
	if(argv[0][namelen-1] == ':')
	{
	    /* Device-spec. */
	    char hdname[namelen];
	    memcpy(hdname, argv[0], namelen - 1);
	    hdname[namelen-1] = 0;
	    new->is_file = FALSE;
	    new->vdisk.disk = hd->find_partition(hdname);
	    DB(("vide: Opened partition `%s' (%p)\n", hdname, new->vdisk.disk));
	}
	else
	{
	    new->is_file = TRUE;
	    new->vdisk.file = fs->open(argv[0], F_READ | F_WRITE | F_CREATE);
	    DB(("vide: Opened file `%s' (%p)\n", argv[0], new->vdisk.file));
	}
	if((new->is_file && new->vdisk.file)
	   || (!new->is_file && new->vdisk.disk))
	{
	    size_t blocks;
	    if(new->is_file)
	    {
		blocks = ((argc > 1)
			  ? kernel->strtoul(argv[1], NULL, 0)
			  : (F_SIZE(new->vdisk.file) / 512));
	    }
	    else
		blocks = new->vdisk.disk->size;
	    if(!new->is_file
	       && (new->vdisk.disk->size == new->vdisk.disk->hd->total_blocks))
	    {
		/* Using a physical disk as a virtual one, use the physical
		   disk's true geometry. */
		new->cylinders = new->vdisk.disk->hd->cylinders;
		new->heads = new->vdisk.disk->hd->heads;
		new->sectors = new->vdisk.disk->hd->sectors;
	    }
	    else
	    {
		/* Attempt to derive some feasible C/H/S values from a block-
		   count. The constraints are that heads<16, heads%2==0,
		   sectors<63 and cylinders<1024 (for the crap BIOS).. */
		new->sectors = 62;
		blocks /= 62;
		new->heads = 2;
		while((blocks / new->heads) >= 1024)
		    new->heads += 2;
		new->cylinders = blocks / new->heads;
	    }
	    new->blocks = new->cylinders * new->heads * new->sectors;
	    new->select = 0xA0;
	    new->status = RDY_STAT;
	    new->irq = 14;
	    new->kh.func = delete_vide;
	    vm->add_vm_kill_handler(vmach, &new->kh);
	    vmach->slots[vm_slot] = new;

	    if(new->is_file)
		fs->set_file_size(new->vdisk.file, new->blocks * 512);

	    vmach->hardware.total_hdisks = 1;
	    vmach->hardware.hdisk[0].cylinders = new->cylinders;
	    vmach->hardware.hdisk[0].heads = new->heads;
	    vmach->hardware.hdisk[0].sectors = new->sectors;
            if(vmach->hardware.got_cmos == 1) {
                struct vcmos_module *vcmos;
                vcmos = (struct vcmos_module *)kernel->open_module("vcmos", SYS_VER);
                if(vcmos != NULL) {
                    switch(vmach->hardware.total_hdisks) {
                        case 0:
                        vcmos->set_vcmos_byte(vmach, 0x12, 0);
                        vcmos->set_vcmos_byte(vmach, 0x19, 0);
                        vcmos->set_vcmos_byte(vmach, 0x1A, 0);
                        break;

                        case 1:
                        vcmos->set_vcmos_byte(vmach, 0x12, 0xF0);
                        vcmos->set_vcmos_byte(vmach, 0x19, 47);
                        vcmos->set_vcmos_byte(vmach, 0x1A, 0);
                        break;

                        case 2:
                        vcmos->set_vcmos_byte(vmach, 0x12, 0xFF);
                        vcmos->set_vcmos_byte(vmach, 0x19, 47);
                        vcmos->set_vcmos_byte(vmach, 0x1A, 47);
                        break;
                    }
                    kernel->close_module((struct module *)vcmos);
                }
            }

	    vide_module.base.vxd_base.open_count++;

	    vpic->set_mask(vmach, FALSE, 1<<new->irq);

	    return TRUE;
	}
	kernel->free(new);
    }
    return FALSE;
}


/* I/O port virtualisation. */

static inline bool
read_one_block(struct vide *v, u_long blkno, void *buf)
{
    if(v->is_file)
    {
	return ((fs->seek(v->vdisk.file, blkno * 512, SEEK_ABS) >= 0)
		&& (fs->read(buf, 512, v->vdisk.file) == 512));
    }
    else
	return hd->read_blocks(v->vdisk.disk, buf, blkno, 1);
}

static inline bool
write_one_block(struct vide *v, u_long blkno, void *buf)
{
    if(v->is_file)
    {
	return ((fs->seek(v->vdisk.file, blkno * 512, SEEK_ABS) >= 0)
		&& (fs->write(buf, 512, v->vdisk.file) == 512));
    }
    else
	return hd->write_blocks(v->vdisk.disk, buf, blkno, 1);
}

static inline void
make_irq(struct vide *v)
{
    v->intrq = TRUE;
    if((v->devctrl & HD_nIEN) == 0)
	vpic->simulate_irq(v->vm, v->irq);
}

static void
read_next_block(struct vide *v)
{
    if(v->block >= v->blocks)
    {
	kprintf("vide: Reading a non-existent block, vm=%p block=%d\n",
		v->vm, v->block);
	v->error = HD_ERR_UNK;
	v->status = ERR_STAT;
    }
    else
    {
	/* It would be nice to have async I/O here.. */
	if(!read_one_block(v, v->block, v->buf))
	{
	    v->error = HD_ERR_UNK;
	    v->status = ERR_STAT;
	}
	else
	{
	    v->status = DATA_RDY_STAT;
	    v->block++;
	    v->blocks_left--;
	    v->buf_index = 0;
	    make_irq(v);
	}
    }
}

static void
write_block(struct vide *v)
{
    if(v->block >= v->blocks)
    {
	kprintf("vide: Writing a non-existent block, vm=%p block=%d\n",
		v->vm, v->block);
	v->error = HD_ERR_UNK;
	v->status = WERR_STAT;
    }
    else
    {
	/* It would be nice to have async I/O here.. */
	if(!write_one_block(v, v->block, v->buf))
	{
	    v->error = HD_ERR_BBK;
	    v->status = WERR_STAT;
	}
	else
	{
	    v->block++;
	    if(--v->blocks_left > 0)
	    {
		v->status = DATA_RDY_STAT;
		make_irq(v);
	    }
	    v->buf_index = 0;
	}
    }
}

static u_long
vide_in(struct vm *vm, u_short port, int size)
{
    struct vide *v = vm->slots[vm_slot];
    kprintf("vide_in: v=%p port=%x size=%d\n", v, port, size);
    if(v == NULL)
	return (u_long)-1;
    switch(port)
    {
    case HD_DATA:
	{
	    u_long val;
	    switch(size)
	    {
	    case 1:
		val = v->buf[v->buf_index++];
		break;
	    case 2:
		val = *((u_short *)(v->buf + v->buf_index));
		v->buf_index += 2;
		break;
	    case 4:
	    default: /* Remove gcc's warning. */
		val = *((u_long *)(v->buf + v->buf_index));
		v->buf_index += 4;
	    }
	    if(v->buf_index >= 256)
	    {
		v->status = RDY_STAT;
		if(v->blocks_left > 0)
		    read_next_block(v);
	    }
	    return val;
	}

    case HD_ERROR:
	return v->error;

    case HD_NSECTOR:
	return v->num_sectors;

    case HD_SECTOR:
	return v->sector;

    case HD_LCYL:
	return v->low_cyl;

    case HD_HCYL:
	return v->high_cyl;

    case HD_CURRENT:
	return v->select;

    case HD_STATUS:
	v->intrq = FALSE;
	return v->status;

    case HD_ALTSTATUS:
	return v->status;
    }
    return (u_long)-1;
}

static inline void
make_blkno(struct vide *v)
{
    v->block = ((v->sector - 1)	
		+ ((v->select & 0x0f) * v->sectors)
		+ (((v->high_cyl << 8) + v->low_cyl) * v->heads * v->sectors));
}

static void
vide_out(struct vm *vm, u_short port, int size, u_long val)
{
    struct vide *v = vm->slots[vm_slot];
    kprintf("vide_out: v=%p port=%x size=%d val=%x\n", v, port, size, val);
    if(v == NULL)
	return;
    switch(port)
    {
    case HD_DATA:
	switch(size)
	{
	case 1:
	    v->buf[v->buf_index++] = val;
	    break;
	case 2:
	    *((u_short *)(v->buf + v->buf_index)) = val;
	    v->buf_index += 2;
	    break;
	case 4:
	    *((u_long *)(v->buf + v->buf_index)) = val;
	    v->buf_index += 4;
	}
	if(v->buf_index >= 256)
	{
	    v->status = RDY_STAT;
	    write_block(v);
	}
	break;

    case HD_FEATURE:
	v->features = val;
	break;

    case HD_NSECTOR:
	v->num_sectors = val;
	break;

    case HD_SECTOR:
	v->sector = val;
	break;

    case HD_LCYL:
	v->low_cyl = val;
	break;

    case HD_HCYL:
	v->high_cyl = val;
	break;

    case HD_CURRENT:
	v->select = val;
	/* Check if they're attempting to select drive 1 (which doesn't
	   exist). */
	v->status = (val & 0x10) ? BUSY_STAT : RDY_STAT;
	break;

    case HD_COMMAND:
	v->command = val;
	switch(val)
	{
	case HD_CMD_RECAL:
	case HD_CMD_SEEK:
	    make_irq(v);
	    break;

	case HD_CMD_READ:
	    make_blkno(v);
	    v->blocks_left = v->num_sectors;
	    read_next_block(v);
	    break;

	case HD_CMD_WRITE:
	    make_blkno(v);
	    v->blocks_left = v->num_sectors;
	    v->status = DATA_RDY_STAT;
	    make_irq(v);
	    break;

	default:
	    kprintf("vide: Unknown command %x, vm=%p\n", v->command, vm);
	    v->status = ERR_STAT;
	    v->error = HD_ERR_ABRT;
	}
	break;

    case HD_DEVCTRL:
	v->devctrl = val;
	if(val & HD_SRST)
	{
	    /* Reset controlller. */
	    v->error = v->num_sectors = v->sector = 0x01;
	    v->low_cyl = v->high_cyl = v->select = 0x00;
	    v->status = RDY_STAT;
	    v->devctrl &= ~HD_SRST;
	}
	break;
    }
}


/* Block access (for VBIOS). */

static inline void
set_stat(struct vide *v, bool ok, bool write, u_char err)
{
    v->status = RDY_STAT | ok ? 0 : (write ? WERR_STAT : ERR_STAT);
    v->error = ok ? 0 : err;
}

static int
read_user_blocks(struct vm *vm, u_int drvno, u_int head, u_int cyl,
		 u_int sector, int count, void *buf)
{
    struct vide *v = vm->slots[vm_slot];
    char tmp_buf[512];
    u_long blkno;
    int actual = 0;
    DB(("vide:read_user_blocks: vm=%p drv=%d head=%d cyl=%d sect=%d count=%d buf=%p\n",
	vm, drvno, head, cyl, sector, count, buf));
    if(v == NULL)
	return -1;
    blkno = (sector - 1) + (head * v->sectors) + (cyl * v->heads * v->sectors);
    if((drvno != 0)
       || (head >= v->heads)
       || (cyl >= v->cylinders)
       || (sector > v->sectors)
       || (blkno + count >= v->blocks))
    {
	ERRNO = E_BADARG;
	set_stat(v, FALSE, FALSE, HD_ERR_IDNF);
	DB(("vide:read_sector: bad argument.\n"));
	return -1;
    }
    while((actual < count) && read_one_block(v, blkno, tmp_buf))
    {
	actual++; blkno++;;
	memcpy_to_user(buf, tmp_buf, 512);
	buf += 512;
    }
    set_stat(v, actual == count, FALSE, HD_ERR_UNK);
    return actual;
}

static int
write_user_blocks(struct vm *vm, u_int drvno, u_int head, u_int cyl,
		  u_int sector, int count, void *buf)
{
    struct vide *v = vm->slots[vm_slot];
    char tmp_buf[512];
    u_long blkno;
    int actual = 0;
    DB(("vide:write_user_blocks: vm=%p drv=%d head=%d cyl=%d sect=%d count=%d buf=%p\n",
	vm, drvno, head, cyl, sector, count, buf));
    if(v == NULL)
	return -1;
    blkno = (sector - 1) + (head * v->sectors) + (cyl * v->heads * v->sectors);
    if((drvno != 0)
       || (head >= v->heads)
       || (cyl >= v->cylinders)
       || (sector > v->sectors)
       || (blkno + count >= v->blocks))
    {
	ERRNO = E_BADARG;
	set_stat(v, FALSE, FALSE, HD_ERR_IDNF);
	DB(("vide:write_sector: bad argument.\n"));
	return -1;
    }
    do {
	memcpy_from_user(tmp_buf, buf, 512);
	buf += 512;
	if(!write_one_block(v, blkno, tmp_buf))
	    break;
	actual++; blkno++;
    } while(actual < count);
    set_stat(v, actual == count, TRUE, HD_ERR_UNK);
    return actual;
}

static bool
get_status(struct vm *vm, u_char *statp, u_char *errp)
{
    struct vide *v = vm->slots[vm_slot];
    if(v == NULL)
	return FALSE;
    *statp = v->status;
    *errp = v->error;
    return TRUE;
}

static bool
get_geom(struct vm *vm, u_int *headsp, u_int *cylsp, u_int *sectsp)
{
    struct vide *v = vm->slots[vm_slot];
    if(v == NULL)
	return FALSE;
    *headsp = v->heads;
    *cylsp = v->cylinders;
    *sectsp = v->sectors;
    return TRUE;
}


/* Module stuff. */

static struct io_handler low_ioh = {
    NULL, "ide", HD_DATA, HD_STATUS, vide_in, vide_out
};

static struct io_handler high_ioh = {
    NULL, "ide", HD_DEVCTRL, HD_DEVCTRL, vide_in, vide_out
};

static bool
vide_init(void)
{
    fs = (struct fs_module *)kernel->open_module("fs", SYS_VER);
    if(fs != NULL)
    {
	hd = (struct hd_module *)kernel->open_module("hd", SYS_VER);
	if(hd != NULL)
	{
	    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
	    if(vm != NULL)
	    {
		vpic = (struct vpic_module *)kernel->open_module("vpic", SYS_VER);
		if(vpic != NULL)
		{
		    vm_slot = vm->alloc_vm_slot();
		    if(vm_slot >= 0)
		    {
			vm->add_io_handler(NULL, &low_ioh);
			vm->add_io_handler(NULL, &high_ioh);
			return TRUE;
		    }
		    kernel->close_module((struct module *)vpic);
		}
		kernel->close_module((struct module *)vm);
	    }
	    kernel->close_module((struct module *)hd);
	}
	kernel->close_module((struct module *)fs);
    }
    return FALSE;
}

static bool
vide_expunge(void)
{
    if(vide_module.base.vxd_base.open_count == 0)
    {
	vm->remove_io_handler(NULL, &high_ioh);
	vm->remove_io_handler(NULL, &low_ioh);
	vm->free_vm_slot(vm_slot);
	kernel->close_module((struct module *)vpic);
	kernel->close_module((struct module *)vm);
	kernel->close_module((struct module *)hd);
	kernel->close_module((struct module *)fs);
	return TRUE;
    }
    return FALSE;
}

struct vide_module vide_module =
{
    { MODULE_INIT("vide", SYS_VER, vide_init, NULL, NULL, vide_expunge),
      create_vide },
    delete_vide, read_user_blocks, write_user_blocks, get_status, get_geom
};
