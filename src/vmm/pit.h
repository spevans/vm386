/* pit.h -- Timer bit definitions.
   John Harper. */

#ifndef __VMM_PIT_H
#define __VMM_PIT_H

/* Command bits. */
#define PIT_CMD_BCD		0x01
#define PIT_CMD_MODE		0x0e
#define PIT_CMD_MODE0		0x00
#define PIT_CMD_MODE1		0x02
#define PIT_CMD_MODE2		0x04
#define PIT_CMD_MODE3		0x06
#define PIT_CMD_MODE4		0x08
#define PIT_CMD_MODE5		0x0a
#define PIT_CMD_LATCH		0x30
#define PIT_CMD_LATCH_LSB	0x10
#define PIT_CMD_LATCH_MSB	0x20
#define PIT_CMD_LATCH_LSB_MSB	0x30
#define PIT_CMD_CHANNEL		0xc0
#define PIT_CMD_CHANNEL0	0x00
#define PIT_CMD_CHANNEL1	0x40
#define PIT_CMD_CHANNEL2	0x80
#define PIT_CMD_READ_BACK	0xc0

#endif /* __VMM_PIT_H */
