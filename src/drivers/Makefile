
SUBDIRS := physical virtual

all :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

clean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
