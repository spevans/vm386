
SRCS = cmds.c dparam.c fd-fs.c fd_mod.c fdc.c floppy.c
OBJS = $(SRCS:.c=.o)

all : fd.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DFD_MODULE

fd.module : $(OBJS)

clean :
	rm -f *.[od] *~ fd.module *.map

include $(SRCS:.c=.d)
