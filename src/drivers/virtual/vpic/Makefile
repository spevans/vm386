
C_SRCS = vpic.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vpic.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVPIC_MODULE

vpic.module : $(OBJS)

clean :
	rm -f *~ *.[ods] *.module

include $(C_SRCS:.c=.d)
