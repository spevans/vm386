
NAME = vmm-tech
SRCS = vmm-tech.texi kernel.texi devs.texi vdevs.texi fs.texi debug.texi \
       types.texi loader.texi shell.texi utils.texi intro.texi devel.texi \
       syslogd.texi spooler.texi dma.texi floppy.texi ramdisk.texi \
       vprinter.texi vdma.texi

MAKEINFO = makeinfo

#MAKEINFOFLAGS = --no-validate
MAKEINFOFLAGS = 

TEXI2DVI = texi2dvi
DVIPS = dvips

all : $(NAME).ps

info : $(NAME).info

doc : $(NAME).doc

$(NAME).info : $(SRCS)
	$(MAKEINFO) $(MAKEINFOFLAGS) $(NAME).texi -o $(NAME).info

$(NAME).doc : $(SRCS)
	$(MAKEINFO) $(MAKEINFOFLAGS) --no-headers --no-split $(NAME).texi -o $(NAME).doc

$(NAME).dvi : $(SRCS)
	$(TEXI2DVI) $(NAME).texi

$(NAME).ps : $(NAME).dvi
	$(DVIPS) $(NAME).dvi

man : $(NAME).info $(NAME).doc $(NAME).dvi $(NAME).ps

clean realclean :
	rm -f *~ *.info* *.??? *.??

tar :
	tar cf - `find . '(' -name '*.texi'				\
			     -o -name '*.info*'				\
			     -o -name '*.doc'				\
			     -o -name '[A-Z]*' ')'			\
			  -a '!' -name '*~' -print ` | gzip -c >tech-man.tgz

nobak :
	rm -f *~
