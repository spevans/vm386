
C_SRCS = vdma.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vdma.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vdma.module : $(OBJS)

vdma.o : vdma.c

clean :
	rm -f *.[odsh] *.module *~ *.map 

include $(C_SRCS:.c=.d)
