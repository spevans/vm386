#ifndef __DMA_H__
#define __DMA_H__

/* DMA allocation and deallocation functions 
   Simon Evans */

struct DmaBuf {
        char    *Buffer;
        u_int8  Page;
        u_int16 Offset;
        u_int16 Len;
        u_int8  Chan;
};

extern bool alloc_dmachan(unsigned int chan, char *name);
extern void dealloc_dmachan(unsigned int chan);
struct shell;
extern void describe_dma(struct shell *sh);
extern void setup_dma(struct DmaBuf *, u_int8);

#define DMA_READ        0x48    /* mem -> device */
#define DMA_WRITE       0x44    /* device -> mem */

#endif
