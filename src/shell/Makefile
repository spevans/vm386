
SRCS = shell.c command.c cmds.c
OBJS = $(SRCS:.c=.o)

all : shell.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

CFLAGS += -DSHELL_MODULE

shell.module : $(OBJS)

clean :
	rm -f *~ *.[od] shell.module core *.map

include $(SRCS:.c=.d)
