
SRCS = cga.c mda.c video.c vga.c
OBJS = $(SRCS:.c=.o)

all : video.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVIDEO_MODULE

video.module : $(OBJS)

clean :
	rm -f *.[od] *~ *.module *.map

include $(SRCS:.c=.d)
