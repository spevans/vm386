SRCS = cmds.c ramdisk_mod.c ramdisk.c
OBJS = $(SRCS:.c=.o)

all : ramdisk.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

ramdisk.module : $(OBJS)

clean :
	rm -f *.[od] *~ ramdisk.module *.map

include $(SRCS:.c=.d)
