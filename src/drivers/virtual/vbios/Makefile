
SRCS = vbios.c video.c disk.c misc.c
OBJS = $(SRCS:.c=.o) code16.o

16_SRCS = init.S16 kbd.S16 disk.S16 timer.S16
16_OBJS = $(16_SRCS:.S16=.o16)

all : vbios.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVBIOS_MODULE

vbios.module : $(OBJS)

code16.c : $(16_OBJS)
	ld86 -0 -s $^ -o code16.out
	$(TOPDIR)/tools/bbin16 code16.out code16
	$(TOPDIR)/tools/btoa vbios_code16 <code16 >code16.c

clean :
	rm -f *.[odsh] *.module *~ *.map code16* *.o16

include $(SRCS:.c=.d)
