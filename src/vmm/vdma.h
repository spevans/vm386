/* vdma.h -- Definitions for the virtual DMA. */

#ifndef __VMM_VDMA_H
#define __VMM_VDMA_H

#include <vmm/types.h>
#include <vmm/module.h>
#include <vmm/vm.h>

#define	CHAN0_ADDR	0
#define CHAN0_COUNT	1
#define	CHAN1_ADDR	2
#define CHAN1_COUNT	3
#define	CHAN2_ADDR	4
#define CHAN2_COUNT	5
#define	CHAN3_ADDR	6
#define CHAN3_COUNT	7
#define STSCMD		8
#define REQUEST		9
#define CHANMASK	10
#define DMAMODE		11
#define CLEAR_FLIPFLOP	12
#define MASTER_TEMP	13
#define CLEAR_MASK	14
#define MULTIMASK	15

/* information about one channel */

struct channel_info {
    u_int8	page;
    u_int16	address;
    u_int16	len;
    u_int8      mode;
};

/* internal use information about one channel */

struct dma_chan {
    u_int8	page;
    u_int16	address;
    u_int16	len;
    bool	addr_l;
    bool	len_l;
};

/* Master structure for both chips */

struct dma_ctrl {
    struct dma_chan  chan[4];
    u_int8  stscmd;
    u_int8  req;
    u_int8  mask;
    u_int8  mode;
    u_int8  clr_ff;
    u_int8  temp;
    u_int8  clr_mask;
    u_int8  multi_reg;
};
    
struct vdma {
    struct vm *vm;
    struct vm_kill_handler kh;
    struct dma_ctrl dmac[2];
};

struct vdma_module {
    struct vxd_module base;
    void (*delete_vdma)(struct vm *vm);
    void (*get_dma_info)(struct vm *vm, u_int channel, struct channel_info *info);
    void (*set_dma_info)(struct vm *vm, u_int channel, struct channel_info *info);
};

extern struct vdma_module vdma_module;

#define	DMA0_LOW	0x00
#define DMA0_HI		0x0F
#define DMA1_LOW	0xC0
#define	DMA1_HI		0xDF
#define DMAPAGE_LOW	0x80
#define DMAPAGE_HI	0x90

#endif /* __VMM_VDMA_H */
