

SRCS = kbd.c cooked.c kbd_mod.c
OBJS = $(SRCS:.c=.o)

all : kbd.module

TOPDIR = ../../..

include $(TOPDIR)/Makedefs

CFLAGS += -DKBD_MODULE

kbd.module : $(OBJS)

clean :
	rm -f *.[od] *~ *.map *.module

include $(SRCS:.c=.d)
