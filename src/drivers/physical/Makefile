
SUBDIRS := hd kbd tty ramdisk fd

all :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

clean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
