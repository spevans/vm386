
SRCS = tty.c readline.c
OBJS = $(SRCS:.c=.o)

all : tty.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

tty.module : $(OBJS)

CFLAGS += -DTTY_MODULE

clean :
	rm -f *.[od] *.module *~ *.map

include $(SRCS:.c=.d)
