
C_SRCS = vprinter.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vprinter.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vprinter.module : $(OBJS)

vprinter.o : vprinter.c

clean :
	rm -f *.[odsh] *.module *~ *.map 

include $(C_SRCS:.c=.d)
