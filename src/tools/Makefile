# Makefile for the tools dir

TOPDIR = ..
CC := gcc -I$(TOPDIR)
CPP := cpp -I$(TOPDIR)
CFLAGS := -g -std=gnu99 -Werror -Wall -Wextra -Wstrict-prototypes -m32 -march=i386

%.d : %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed '\''s/$*.o/& $@/g'\'' > $@'

SRCS = e2b.c disasm.c bbin.c bbin16.c makeimage.c bsc.c sysdisk.c btoa.c sbb.c mld-elf.c mdump.c
TOOLS := e2b bbin bbin16 makeimage disasm bsc sysdisk btoa sbb mld-elf mdump

all : $(TOOLS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^


mld-elf: mld-elf.c elf_file.c
	$(CC) $(CFLAGS) -o $@ $^

clean :
	rm -f *~ *.[od] $(TOOLS)

include $(SRCS:.c=.d)
