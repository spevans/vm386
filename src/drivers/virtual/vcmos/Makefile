
C_SRCS = vcmos.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vcmos.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vcmos.module : $(OBJS)

vcmos.o : vcmos.c

clean :
	rm -f *.[odsh] *.module *~ *.map 

include $(C_SRCS:.c=.d)
