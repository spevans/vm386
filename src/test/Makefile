SRCS = test_mod.c # test.c
OBJS = $(SRCS:.c=.o)

all : test.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

test.module : $(OBJS)

clean :
	rm -f *.[od] *~ test.module *.map

include $(SRCS:.c=.d)
