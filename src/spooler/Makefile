C_SRCS = spooler.c spooler_mod.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : spooler.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

spooler.module : $(OBJS)

clean :
	rm -f *.[odsh] *.module *~ *.map 

include $(C_SRCS:.c=.d)
