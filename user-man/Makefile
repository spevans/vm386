
NAME = user-man
SRCS = user-man.texi debug.texi fs.texi intro.texi kernel.texi \
       shell.texi vm.texi overview.texi vfloppy.texi vprinter.texi \
       ramdisk.texi devs.texi floppy.texi syslogd.texi spooler.texi

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
			  -a '!' -name '*~' -print ` | gzip -c >$(NAME).tgz

nobak :
	rm -f *~
