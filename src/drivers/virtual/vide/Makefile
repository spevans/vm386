
SRCS = vide.c
OBJS = $(SRCS:.c=.o)

all : vide.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

vide.module : $(OBJS)

CFLAGS += -DVIDE_MODULE

clean :
	rm -f *.[od] *.module *~ *.map

include $(SRCS:.c=.d)
