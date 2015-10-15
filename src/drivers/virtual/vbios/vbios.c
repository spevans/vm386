/* vbios.c -- Virtual BIOS.
   John Harper. */

#include <vmm/vbios.h>
#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/string.h>
#include <vmm/shell.h>

#define kprintf kernel->printf

static struct vm_module *vm;

static int vm_slot;

static u_char top32_bytes[32] =
    "\12\0\374\1\0\340\0\0\0\0\0\0\0\0\0\0"	/* Config table */
    "\352\0\0\0\377"				/* JMPF 0xff00,0 */
    "02/26/95\0"				/* date */
    "\374";					/* PC AT */


static void
delete_vbios(struct vm *vmach)
{
    struct vbios *v = vmach->slots[vm_slot];
    if(v != NULL)
    {
	vmach->slots[vm_slot] = NULL;
	kernel->free_page(v->code);
	kernel->free(v);
	vbios_module.base.vxd_base.open_count--;
    }
}

static bool
create_vbios(struct vm *vmach, __attribute__ ((unused)) int argc,
             __attribute__ ((unused))char **argv)
{
    struct vbios *new = kernel->calloc(sizeof(struct vbios), 1);
    if(new != NULL)
    {
	new->vm = vmach;
	new->code = kernel->alloc_page();
	if(new->code != NULL)
	{
	    u_long x;
	    vmach->slots[vm_slot] = new;
	    /* Copy the VBIOS 16-bit code into its page.. */
	    memcpy(new->code, vbios_code16, vbios_code16_length);
	    /* ..then setup the top-most 32 bytes of memory (sys-config
	       table, jmp to init, date, ID etc.. */
	    memcpy(((char *)new->code) + (4096 - 32), top32_bytes, 32);
	    for(x = 0xfc000; x < 0x100000; x += PAGE_SIZE)
	    {
		/* Map our code into each page from fc00:0 to 10000:0
		   just in case.. */
		kernel->map_page(vmach->task->page_dir, new->code, x,
				 PTE_USER | PTE_PRESENT);
	    }
	    new->kh.func = delete_vbios;
	    vm->add_vm_kill_handler(vmach, &new->kh);
	    vbios_module.base.vxd_base.open_count++;
            init_vbios_misc(vmach, vm);
	    return TRUE;
	}
	kernel->free(new);
    }
    return FALSE;
}



static void
init_hdinfo(int vector, struct hd_info *info)
{
    char *user_ptr = (char *)(get_user_short((u_short *)(vector * 4))
			      + (get_user_short((u_short *)(vector * 4 + 2)) << 4));
    memcpy_to_user(user_ptr, info, sizeof(struct hd_info));
}

void
vbios_arpl_handler(struct vm *vmach, struct vm86_regs *regs, u_short num)
{
    DB(("vbios_arpl_handler: vm=%p num=%#x\n", vm, num));
    switch(num)
    {
    case 0x0f:			/* Setup BIOS data area. */
	memset_user(BIOS_DATA, 0, 0x100);
	put_user_short(0x0030, &BIOS_DATA->equipment_data);
	put_user_short(vmach->hardware.base_mem, &BIOS_DATA->low_memory);
	put_user_short(KBD_BUF, &BIOS_DATA->kbd_buf_head);
	put_user_short(KBD_BUF, &BIOS_DATA->kbd_buf_tail);
	put_user_byte(vmach->tty->video.mode, &BIOS_DATA->display_mode);
	put_user_short(vmach->tty->video.mi.cols, &BIOS_DATA->display_cols);
	put_user_short(vmach->tty->video.mi.page_size * vmach->tty->video.mi.pages,
		       &BIOS_DATA->display_buf_size);
	put_user_short(vmach->tty->video.mi.page_size * vmach->tty->current_page,
		       &BIOS_DATA->display_buf_start);
	put_user_byte(vmach->tty->current_page, &BIOS_DATA->active_page);
	put_user_short(vmach->tty->video.vga.crt_i, &BIOS_DATA->crt_base);
	put_user_short(KBD_BUF, &BIOS_DATA->kbd_buf_start);
	put_user_short(KBD_BUF + 32, &BIOS_DATA->kbd_buf_end);
	put_user_byte(vmach->tty->video.mi.rows - 1,
		      &BIOS_DATA->display_rows);
	if(!strcasecmp(vmach->tty->video.ops->name, "mda"))
	    put_user_byte(0x02, &BIOS_DATA->video_control_states[0]);
	if(vmach->hardware.total_hdisks > 0)
	{
	    init_hdinfo(0x41, &vmach->hardware.hdisk[0]);
	    if(vmach->hardware.total_hdisks > 1)
		init_hdinfo(0x46, &vmach->hardware.hdisk[1]);
	}
        memcpy_to_user(&BIOS_DATA->lpt1_base, &(vmach->hardware.lprports), 8);
	break;

    case 0x10:			/* Video functions. */
	vbios_video_handler(vmach, regs);
	break;

    case 0x11:			/* Equipment determination. */
	regs->eax = SET16(regs->eax,
			  get_user_short(&BIOS_DATA->equipment_data));
	break;

    case 0x12:			/* Memory size determination. */
	regs->eax = SET16(regs->eax, get_user_short(&BIOS_DATA->low_memory));
	break;

    case 0x13:			/* Disk functions. */
	vbios_disk_handler(vmach, regs);
	break;

    case 0x14:			/* Serial comms. functions. */
	vbios_ser_handler(vmach, regs);
	break;

    case 0x15:			/* System services. */
	vbios_sys_handler(vmach, regs, vmach->slots[vm_slot]);
	break;

    case 0x17:			/* Printer functions. */
	vbios_par_handler(vmach, regs);
	break;

    case 0x1A:			/* System-timer functions. */
	vbios_timer_handler(vmach, regs);
	break;
    }
}


/* Module stuff. */

struct arpl_handler vbios_arpls1 = {
    NULL, "vbios", 0x0f, 0x15, vbios_arpl_handler
};
struct arpl_handler vbios_arpls2 = {
    NULL, "vbios", 0x17, 0x18, vbios_arpl_handler
};
struct arpl_handler vbios_arpls3 = {
    NULL, "vbios", 0x1a, 0x1a, vbios_arpl_handler
};

static bool
vbios_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
        if (init_vbios_video())
        {
            if (init_vbios_disk())
            {
                vm_slot = vm->alloc_vm_slot();
                if(vm_slot >= 0)
                {
                    vm->add_arpl_handler(&vbios_arpls1);
                    vm->add_arpl_handler(&vbios_arpls2);
                    vm->add_arpl_handler(&vbios_arpls3);
                    return TRUE;
                }
                deinit_vbios_disk();
            }
            deinit_vbios_video();
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vbios_expunge(void)
{
    if(vbios_module.base.vxd_base.open_count == 0)
    {
	vm->remove_arpl_handler(&vbios_arpls3);
	vm->remove_arpl_handler(&vbios_arpls2);
	vm->remove_arpl_handler(&vbios_arpls1);
	vm->free_vm_slot(vm_slot);
        deinit_vbios_misc();
        deinit_vbios_disk();
        deinit_vbios_video();
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vbios_module vbios_module =
{
    { MODULE_INIT("vbios", SYS_VER, vbios_init, NULL, NULL, vbios_expunge),
      create_vbios },
    delete_vbios
};
