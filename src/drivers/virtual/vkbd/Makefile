
SRCS = vkbd.c
OBJS = $(SRCS:.c=.o)

all : vkbd.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVKBD_MODULE

vkbd.module : $(OBJS)

clean :
	rm -f *.[od] *.module *~ *.map

include $(SRCS:.c=.d)
