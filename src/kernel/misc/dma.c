/* DMA allocation and deallocation functions 
 * Simon Evans
 * Tested and corrected C.Luke
 */

#include <vmm/types.h>
#include <vmm/dma.h>
#include <vmm/kernel.h>
#include <vmm/io.h>
#include <vmm/shell.h>

/* Mapping of allocated channels. 4 is preset since it's the cascase */
static char  dma_chans[8] = {0, 0, 0, 0, 1, 0, 0, 0 };
static char *dma_names[8] = {NULL, NULL, NULL, NULL, "cascade", NULL, NULL, NULL};

struct dma_addr
{
	int Page;
	int Address;
	int Len;
};

static struct dma_addr DmaPorts[] =
{
	{0x87,  0x0,	0x1	},  /* Channel 0 */
	{0x83,  0x2,	0x3	},  /* Channel 1 */
	{0x81,  0x4,	0x5	},  /* Channel 2 */
	{0x82,  0x6,	0x7	},  /* Channel 3 */
	{0x8F,  0xC0,   0xC2	},  /* Channel 4 */
	{0x8B,  0xC4,   0xC6	},  /* Channel 5 */
	{0x89,  0xC8,   0xCA	},  /* Channel 6 */
	{0x8A,  0xCC,   0xCE	}   /* Channel 7 */
};


bool alloc_dmachan(unsigned int chan, char *name)
{
	u_long flags;

	if(chan > 7)
		return FALSE;

	save_flags(flags);
	cli();

	if(dma_chans[chan] == 0)
	{
		dma_chans[chan] = 1;
		dma_names[chan] = name;

		load_flags(flags);
		return TRUE;
	}

	load_flags(flags);
	return FALSE;
}

void dealloc_dmachan(unsigned int chan)
{
	if(chan > 7)
		return;

	dma_names[chan] = NULL;
	dma_chans[chan] = 0;
}
   
 
void describe_dma(struct shell *sh)
{
	int i;
	for(i = 0; i < 8; i++)
	{
		if(dma_chans[i] || dma_names[i])
		{
			sh->shell->printf(sh, "Channel%-2d   %s\n", i,
				dma_names[i] == NULL ? "" : dma_names[i]);
		}
	}
}


void setup_dma(struct DmaBuf *DmaInfo, u_int8 Mode)
{
	struct dma_addr *DmaPort;
	u_int8   Page;
	u_int16  Offset;
	u_int16  Len;
	u_int8   Chan;

	Chan = DmaInfo->Chan;
	Page = DmaInfo->Page;
	Offset = DmaInfo->Offset;
	Len = DmaInfo->Len;

	DmaPort = &DmaPorts[Chan];

	kprintf("DMA: Chan:%01X Buf:%p PBuf:%08lX Page:%02X Offset:%04X Len:%04X\n",
		Chan, DmaInfo->Buffer, TO_PHYSICAL(DmaInfo->Buffer),
		Page, Offset, Len);

	if(Chan < 4)
	{
		/* Set the mask register */
		outb(Chan | 4, 0xA);

		/* Clear the flip flop */
		outb(0, 0xC);

		/* Set the transfer mode */
		outb(Chan | Mode, 0xB);
	}
	else
	{
		/* Shift things for a word transfer */
		Offset >>= 1;
		Len >>= 1;

		/* Set the mask register */
		outb(Chan, 0xD4);

		/* Clear the flip flop */
		outb(0, 0xD8);

		/* Set the transfer mode */
		outb((Chan & 3) | Mode, 0xD6);
	}

	/* Load the address... Low-byte first */
	outb(Offset & 0xff, DmaPort->Address);
	Offset >>= 8;
	outb(Offset & 0xff, DmaPort->Address);

	/* Load the transfer length.. Low-byte first */
	outb(Len & 0xff, DmaPort->Len);
	Len >>= 8;
	outb(Len & 0xff, DmaPort->Len);

	/* Now the page, this is another 8-bits of address */
	outb(Page, DmaPort->Page);

	if(Chan < 4)
	{
		/* Unmask the channel */
		outb(Chan, 0xA);
	}
	else
	{
		/* Unmask the channel */
		outb(Chan & 3, 0xD4);
	}
}

