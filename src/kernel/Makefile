
SUBDIRS = init alloc misc mm traps tasks modules
ARCHIVES := $(foreach dir, $(SUBDIRS), $(dir)/$(dir).o)

TOPDIR = ..
include $(TOPDIR)/Makedefs

all : ../output/kernel.bin kernel.map

ifdef LD_DOES_QMAGIC
LDFLAGS = -qmagic -Ttext 0x3fe0
else
LDFLAGS = -Ttext 0x4000
endif


.PHONY : kernel.o
kernel.o :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done
	ld $(LDFLAGS) init/startup.o $(OBJS) $(ARCHIVES) -o kernel.o

../output/kernel.bin : kernel.o ../tools/e2b
	../tools/e2b kernel.o ../output/kernel.bin

clean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	rm -f *.[od] *~ *.map
