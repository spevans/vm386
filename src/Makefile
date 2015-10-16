
# The following two variables define your root partition for VMM. They
# should be set by your .profile or similar.
# export VMM_ROOT = hda7:
# export VMM_ROOT_SIZE = 23435

# the following is patched into the bootsector as the boot device
# 0 = drive a 128 = fixed disk 1 129 = fixed disk 2 etc
# only works for floppy at the mo

FS = ../tools/fs

SUBDIRS := tools start16 drivers syslogd spooler fs shell debugger test kernel

TOPDIR = .
include $(TOPDIR)/Makedefs

all : kernel

.PHONY : kernel image sys disk root install rclean clean realclean nobak tar

kernel :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

image :
	tools/makeimage output/bootsect.bin output/start16.bin		\
	 output/kernel.bin $(VMM_BOOT) > image 

sys :
	tools/sysdisk output/start16.bin output/kernel.bin		\
	 $(VMM_ROOT) /dev/$(subst :,,$(VMM_BOOT))

imgsys:
	tools/sysdisk output/start16.bin output/kernel.bin              \
	 fd0 root.image

disk : image
	dd if=image of=/dev/fd0 bs=512

root:
	(echo "cd tst:" ; echo "mkdir lib" ; echo "mkdir spool" ; \
         echo "mkdir adm") | \
	$(FS) -f /dev/$(subst :,,$(VMM_ROOT)) -s $(VMM_ROOT_SIZE) -m -r 256

getlog: 
	(echo "cd tst:" ; echo "cd adm"; echo "type syslog") | \
	$(FS) -f /dev/$(subst :,,$(VMM_ROOT)) -s $(VMM_ROOT_SIZE) | \
	tail +4 > log

usefs:
	$(FS) -f /dev/$(subst :,,$(VMM_ROOT)) -s $(VMM_ROOT_SIZE)

geterr:
	(echo "cd tst:" ; echo "type err") |	\
	$(FS) -f /dev/$(subst :,,$(VMM_ROOT)) -s $(VMM_ROOT_SIZE) >err

imgroot:
	$(FS) -f root.image -s $(VMM_ROOT_SIZE) -m -r 256

install :
	(echo "cd tst:" ;				\
	 sed -e 's/.*\/\([a-z]*.module\)/ucp \0 \/lib\/\1/' <DYNAMIC)	\
	| $(FS) -f /dev/$(subst :,,$(VMM_ROOT)) -s $(VMM_ROOT_SIZE)

imginst:
	(echo "cd tst:" ; echo "mkdir lib";				\
	 sed -e 's/.*\/\([a-z]*.module\)/ucp \0 \/lib\/\1/' <DYNAMIC)	\
	| $(FS) -f root.image -s $(VMM_ROOT_SIZE)

clean :
	find . '(' -name '*~'						\
		   -o -name '*.[oad]'					\
		   -o -name '*.asm'					\
		   -o -name '*.module'					\
		   -o -name '*.map'					\
		   -o -name '*.bin'					\
		   -o -name '*.out'					\
		   -o -name 'err'					\
		   -o -name core ')' -print | xargs rm -f
	make -C tools clean
	make -C drivers clean
	make -C kernel clean
	make -C fs clean

rclean :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	rm -f image

realclean : rclean clean

nobak :
	find . -name '*~' -print | xargs rm -f

tar :
	tar cf - `find . '(' -name '*.[chS]'				\
			     -o -name '[A-Z]*'				\
			     -o -name '*.S16' ')'			\
			  -print ` | gzip -c >src.tgz

count :
	wc  `find . '(' -name '*.[chS]' -o -name '*.S16' ')' -print `
