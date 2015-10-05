/* vdma.c -- Virtual DMA. */

/*
 * basic emulation.
 *
 */

#if 1
  #define DEBUG
#endif

#include <vmm/kernel.h>
#include <vmm/tasks.h>
#include <vmm/vm.h>
#include <vmm/vdma.h>

#define kprintf kernel->printf

struct kernel_module *kernel;
struct vm_module *vm;

static int vm_slot;


static void
delete_vdma(struct vm *vm)
{
    struct vdma *v = vm->slots[vm_slot];
    if(v != NULL)
    {
	vm->slots[vm_slot] = NULL;
	kernel->free(v);
	vdma_module.base.vxd_base.open_count--;
    }
}

static bool
create_vdma(struct vm *vmach, int argc, char **argv)
{
    struct vdma *new = kernel->calloc(sizeof(struct vdma), 1);
    if(new != NULL)
    {
	vdma_module.base.vxd_base.open_count++;
	new->kh.func = delete_vdma;
	vm->add_vm_kill_handler(vmach, &new->kh);
	vmach->slots[vm_slot] = new;
        vmach->hardware.got_dma = 1;
	new->vm = vmach;
        return TRUE;
    }
    return FALSE;
}


/* Virtualisation Stuff */

static u_long
vdma_in(struct vm *vm, u_short port, int size)
{
    struct vdma *vdma = vm->slots[vm_slot];
    struct dma_ctrl *dma = NULL;
    if(vdma == NULL) return -1;

    /* isolate what it wants */

    if(port <= DMA0_HI) {
        dma = &(vdma->dmac[0]);
    } else if (port >= DMA1_LOW) {
        dma = &(vdma->dmac[1]);
        port -= 0xC0;
        port >>= 1;
    } 

    else {
        switch(port) {
  
        case 0x81:
            return vdma->dmac[0].chan[0].page;
          
        case 0x82:
            return vdma->dmac[0].chan[1].page;
          
        case 0x83:
            return vdma->dmac[0].chan[2].page;
          
        case 0x87:
            return vdma->dmac[0].chan[3].page;
          
        case 0x89:
            return vdma->dmac[1].chan[0].page;
          
        case 0x8A:
            return vdma->dmac[1].chan[1].page;
          
        case 0x8B:
            return vdma->dmac[1].chan[2].page;
          
        case 0x8F:
            return vdma->dmac[1].chan[3].page;
        }
    }

    switch(port) {
        case CHAN0_ADDR:
            if(dma->chan[0].addr_l == TRUE) {
                dma->chan[0].addr_l = FALSE;
                return (dma->chan[0].address & 0xFF);
            } else {
                dma->chan[0].addr_l = TRUE;
                return (dma->chan[0].address >> 8) & 0xFF;
            }

        case CHAN0_COUNT:
            if(dma->chan[0].len_l == TRUE) {
                dma->chan[0].len_l = FALSE;
                return (dma->chan[0].len & 0xFF);
            } else {
                dma->chan[0].len_l = TRUE;
                return (dma->chan[0].len >> 8) & 0xFF;
            }

        case CHAN1_ADDR:
            if(dma->chan[1].addr_l == TRUE) {
                dma->chan[1].addr_l = FALSE;
                return (dma->chan[1].address & 0xFF);
            } else {
                dma->chan[1].addr_l = TRUE;
                return (dma->chan[1].address >> 8) & 0xFF;
            }

        case CHAN1_COUNT:
            if(dma->chan[1].len_l == TRUE) {
                dma->chan[1].len_l = FALSE;
                return (dma->chan[1].len & 0xFF);
            } else {
                dma->chan[1].len_l = TRUE;
                return (dma->chan[1].len >> 8) & 0xFF;
            }

        case CHAN2_ADDR:
            if(dma->chan[2].addr_l == TRUE) {
                dma->chan[2].addr_l = FALSE;
                return (dma->chan[2].address & 0xFF);
            } else {
                dma->chan[2].addr_l = TRUE;
                return (dma->chan[2].address >> 8) & 0xFF;
            }

        case CHAN2_COUNT:
            if(dma->chan[2].len_l == TRUE) {
                dma->chan[2].len_l = FALSE;
                return (dma->chan[2].len & 0xFF);
            } else {
                dma->chan[2].len_l = TRUE;
                return (dma->chan[2].len >> 8) & 0xFF;
            }

        case CHAN3_ADDR:
            if(dma->chan[3].addr_l == TRUE) {
                dma->chan[3].addr_l = FALSE;
                return (dma->chan[3].address & 0xFF);
            } else {
                dma->chan[3].addr_l = TRUE;
                return (dma->chan[3].address >> 8) & 0xFF;
            }

        case CHAN3_COUNT:
            if(dma->chan[3].len_l == TRUE) {
                dma->chan[3].len_l = FALSE;
                return (dma->chan[3].len & 0xFF);
            } else {
                dma->chan[3].len_l = TRUE;
                return (dma->chan[3].len >> 8) & 0xFF;
            }

        case STSCMD:
            return dma->stscmd;

        case REQUEST:
            return dma->req;

        case CHANMASK:
            return dma->mask;

        case DMAMODE:
            return dma->mode;

        case CLEAR_FLIPFLOP:
            return dma->clr_ff;

        case MASTER_TEMP:
            return dma->temp;

        case CLEAR_MASK:
            return dma->clr_mask;

        case MULTIMASK:
            return dma->multi_reg;
    }
    return -1;
}

static void
vdma_out(struct vm *vm, u_short port, int size, u_long val)
{
    struct vdma *vdma = vm->slots[vm_slot];
    struct dma_ctrl *dma = NULL;
    if(vdma == NULL) return;

    /* isolate what it wants */

    if(port <= DMA0_HI) {
        dma = &(vdma->dmac[0]);
    } else if (port >= DMA1_LOW) {
        dma = &(vdma->dmac[1]);
        port -= 0xC0;
        port >>= 1;
    } 

    else {
        switch(port) {
  
        case 0x81:
            vdma->dmac[0].chan[0].page = val;
            break;
          
        case 0x82:
            vdma->dmac[0].chan[1].page = val;
            break;
          
        case 0x83:
            vdma->dmac[0].chan[2].page = val;
            break;
          
        case 0x87:
            vdma->dmac[0].chan[3].page = val;
            break;
          
        case 0x89:
            vdma->dmac[1].chan[0].page = val;
            break;
          
        case 0x8A:
            vdma->dmac[1].chan[1].page = val;
            break;
          
        case 0x8B:
            vdma->dmac[1].chan[2].page = val;
            break;
          
        case 0x8F:
            vdma->dmac[1].chan[3].page = val;
            break;
        }
        return;
    }

    switch(port) {
        case CHAN0_ADDR:
            if(dma->chan[0].addr_l == TRUE) {
                dma->chan[0].addr_l = FALSE;
                dma->chan[0].address &= 0xFF00;
                dma->chan[0].address |= (val & 0xFF);
            } else {
                dma->chan[0].addr_l = TRUE;
                dma->chan[0].address &= 0x00FF;
                dma->chan[0].address |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN0_COUNT:
            if(dma->chan[0].len_l == TRUE) {
                dma->chan[0].len_l = FALSE;
                dma->chan[0].len &= 0xFF00;
                dma->chan[0].len |= (val & 0xFF);
            } else {
                dma->chan[0].len_l = TRUE;
                dma->chan[0].len &= 0x00FF;
                dma->chan[0].len |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN1_ADDR:
            if(dma->chan[1].addr_l == TRUE) {
                dma->chan[1].addr_l = FALSE;
                dma->chan[1].address &= 0xFF00;
                dma->chan[1].address |= (val & 0xFF);
            } else {
                dma->chan[1].addr_l = TRUE;
                dma->chan[1].address &= 0x00FF;
                dma->chan[1].address |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN1_COUNT:
            if(dma->chan[1].len_l == TRUE) {
                dma->chan[1].len_l = FALSE;
                dma->chan[1].len &= 0xFF00;
                dma->chan[1].len |= (val & 0xFF);
            } else {
                dma->chan[1].len_l = TRUE;
                dma->chan[1].len &= 0x00FF;
                dma->chan[1].len |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN2_ADDR:
            if(dma->chan[2].addr_l == TRUE) {
                dma->chan[2].addr_l = FALSE;
                dma->chan[2].address &= 0xFF00;
                dma->chan[2].address |= (val & 0xFF);
            } else {
                dma->chan[2].addr_l = TRUE;
                dma->chan[2].address &= 0x00FF;
                dma->chan[2].address |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN2_COUNT:
            if(dma->chan[2].len_l == TRUE) {
                dma->chan[2].len_l = FALSE;
                dma->chan[2].len &= 0xFF00;
                dma->chan[2].len |= (val & 0xFF);
            } else {
                dma->chan[2].len_l = TRUE;
                dma->chan[2].len &= 0x00FF;
                dma->chan[2].len |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN3_ADDR:
            if(dma->chan[3].addr_l == TRUE) {
                dma->chan[3].addr_l = FALSE;
                dma->chan[3].address &= 0xFF00;
                dma->chan[3].address |= (val & 0xFF);
            } else {
                dma->chan[3].addr_l = TRUE;
                dma->chan[3].address &= 0x00FF;
                dma->chan[3].address |= ((val << 8) & 0xFF00);
            }
            break;

        case CHAN3_COUNT:
            if(dma->chan[3].len_l == TRUE) {
                dma->chan[3].len_l = FALSE;
                dma->chan[3].len &= 0xFF00;
                dma->chan[3].len |= (val & 0xFF);
            } else {
                dma->chan[3].len_l = TRUE;
                dma->chan[3].len &= 0x00FF;
                dma->chan[3].len |= ((val << 8) & 0xFF00);
            }
            break;

        case STSCMD:
            dma->stscmd = val;
            break;

        case REQUEST:
            dma->req = val;
            break;

        case CHANMASK:
            dma->mask = val;
            break;

        case DMAMODE:
            dma->mode = val;
            break;

        case CLEAR_FLIPFLOP:
            dma->clr_ff = val;
            break;

        case MASTER_TEMP:
            dma->temp = val;
            break;

        case CLEAR_MASK:
            dma->clr_mask = val;
            break;

        case MULTIMASK:
            dma->multi_reg = val;
            break;
    }
}


static void
get_dma_info(struct vm *vm, u_int channel, struct channel_info *info)
{
    DB(("get_dma_info: vm = %p channel = %u info = %p\n", vm, channel, info));
}

static void
set_dma_info(struct vm *vm, u_int channel, struct channel_info *info)
{
    DB(("set_dma_info: vm = %p channel = %u info = %p\n", vm, channel, info));
}


/* Module stuff. */

static struct io_handler dma_io[3] = {
    { NULL, "dma", DMA0_LOW, DMA0_HI, vdma_in, vdma_out },
    { NULL, "dma", DMA1_LOW, DMA1_HI, vdma_in, vdma_out },
    { NULL, "dma", DMAPAGE_LOW, DMAPAGE_HI, vdma_in, vdma_out }
};

static bool
vdma_init(void)
{
    vm = (struct vm_module *)kernel->open_module("vm", SYS_VER);
    if(vm != NULL)
    {
	vm_slot = vm->alloc_vm_slot();
	if(vm_slot >= 0)
	{
            vm->add_io_handler(NULL, &dma_io[0]);
            vm->add_io_handler(NULL, &dma_io[1]);
            vm->add_io_handler(NULL, &dma_io[2]);
	    return TRUE;
	}
	kernel->close_module((struct module *)vm);
    }
    return FALSE;
}

static bool
vdma_expunge(void)
{
    if(vdma_module.base.vxd_base.open_count == 0)
    {
        vm->remove_io_handler(NULL, &dma_io[0]);
        vm->remove_io_handler(NULL, &dma_io[1]);
        vm->remove_io_handler(NULL, &dma_io[2]);
	vm->free_vm_slot(vm_slot);
	kernel->close_module((struct module *)vm);
	return TRUE;
    }
    return FALSE;
}

struct vdma_module vdma_module =
{
    { MODULE_INIT("vdma", SYS_VER, vdma_init, NULL, NULL, vdma_expunge),
      create_vdma },
    delete_vdma,
    get_dma_info,
    set_dma_info
};
