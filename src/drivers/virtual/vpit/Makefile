
C_SRCS = vpit.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vpit.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVM_MODULE

vpit.module : $(OBJS)

clean :
	rm -f *~ *.[ods] *.module

include $(C_SRCS:.c=.d)
