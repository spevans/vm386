C_SRCS = cmds.c interrupt.c kernel_mod.c printf.c time.c bits.c dma.c \
         errno.c lib.c 
A_SRCS = irq_entry.S
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : misc.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

misc.o : $(OBJS)
	$(LD) $(LDFLAGS) -r -o misc.o $(OBJS)

clean :
	rm -f *~ *.[od]

include $(C_SRCS:.c=.d)

