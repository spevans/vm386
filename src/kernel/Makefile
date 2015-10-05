
SUBDIRS = init alloc misc mm traps tasks modules
ARCHIVES := $(foreach dir, $(SUBDIRS), $(dir)/$(dir).o)

TOPDIR = ..
include $(TOPDIR)/Makedefs

all : ../output/kernel.bin

.PHONY : ../output/kernel.bin
../output/kernel.bin:
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done
	ld -T linker.script -Map=kernel.map -o $@

clean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	rm -f *.[od] *~ *.map
