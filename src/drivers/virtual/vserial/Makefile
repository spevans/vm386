C_SRCS = vserial.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vserial.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vprinter.module : $(OBJS)

vserial.o : vserial.c

clean :
	rm -f *.[odsh] *.module *~ *.map 

include $(C_SRCS:.c=.d)
