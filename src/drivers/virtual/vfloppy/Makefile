SRCS = vfloppy.c cmds.c
OBJS = $(SRCS:.c=.o)

all : vfloppy.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vfloppy.module : $(OBJS)

clean :
	rm -f *.[od] *.module *~ *.map

include $(SRCS:.c=.d)
