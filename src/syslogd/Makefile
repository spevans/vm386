SRCS = cmds.c syslogd_mod.c syslogd.c
OBJS = $(SRCS:.c=.o)

all : syslogd.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

syslogd.module : $(OBJS)

clean :
	rm -f *.[od] *~ syslogd.module *.map

include $(SRCS:.c=.d)
