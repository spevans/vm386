
SRCS = generic.c hd_mod.c ide.c cmds.c
OBJS = $(SRCS:.c=.o)

all : hd.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DHD_MODULE

hd.module : $(OBJS)

clean :
	rm -f *.[od] *~ hd.module *.map

include $(SRCS:.c=.d)
