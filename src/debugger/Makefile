SRCS = debug_mod.c cmds.c ncode.c
OBJS = $(SRCS:.c=.o)

all : debug.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

CFLAGS += -DDEBUG_MODULE

debug.module : $(OBJS)

clean :
	rm -f *.[od] *~ debug.module test *.map

include $(SRCS:.c=.d)
