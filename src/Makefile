
# The following two variables define your root partition for VMM. They
# should be set by your .profile or similar.
# export VMM_ROOT = hda7:
# export VMM_ROOT_SIZE = 23435

# the following is patched into the bootsector as the boot device
# 0 = drive a 128 = fixed disk 1 129 = fixed disk 2 etc
# only works for floppy at the mo

ROOT_DEV := output/image.hd
FS := shell/shell

SUBDIRS := tools start16 drivers syslogd spooler fs shell debugger test kernel

TOPDIR = .
include $(TOPDIR)/Makedefs

all : kernel

.PHONY : kernel image sys disk root install rclean clean realclean nobak tar

kernel :
	set -e; for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done


installmbr:
	tools/makeimage output/mbr.bin $(ROOT_DEV)

mkfs:
	shell/shell -f $(ROOT_DEV) -m -r 256 </dev/null

bootable:
	tools/sysdisk output/start16.bin output/kernel.bin $(ROOT_DEV)

image: installmbr root install bootable


root:
	(echo "cd tst:" ; echo "mkdir lib" ; echo "mkdir spool" ; \
         echo "mkdir adm") | \
	$(FS) -f $(ROOT_DEV) -m -r 256

getlog: 
	(echo "cd tst:" ; echo "cd adm"; echo "type syslog") | \
	$(FS) -f $(ROOT_DEV) | tail +4 > log

usefs:
	$(FS) -f $(ROOT_DEV)

geterr:
	(echo "cd tst:" ; echo "type err") | \
	$(FS) -f $(ROOT_DEV) >err

imgroot:
	$(FS) -f $(ROOT_DEV)  -m -r 256

install:
	(echo "cd tst:" ;				\
	 sed -e 's/.*\/\([a-z]*.module\)/ucp \0 \/lib\/\1/' <DYNAMIC)	\
	| $(FS) -f $(ROOT_DEV)

imginst:
	(echo "cd tst:" ; echo "mkdir lib";				\
	 sed -e 's/.*\/\([a-z]*.module\)/ucp \0 \/lib\/\1/' <DYNAMIC)	\
	| $(FS) -f root.image

clean :
	find . '(' -name '*~'						\
		   -o -name '*.[oad]'					\
		   -o -name '*.ko'					\
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
