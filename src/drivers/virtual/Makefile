
SUBDIRS := vm vbios vcmos vdma vfloppy vide video vkbd \
vpic vpit vprinter 

all :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

clean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

