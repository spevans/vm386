# Makefile for the tools dir

SRCS = e2b.c disasm.c bbin.c bbin16.c makeimage.c bsc.c sysdisk.c btoa.c sbb.c

all : e2b bbin bbin16 makeimage disasm bsc sysdisk btoa sbb

TOPDIR = ..
include $(TOPDIR)/Makedefs

clean :
	rm -f *~ *.[od] e2b bbin bbin16 makeimage wbb disasm bsc sysdisk btoa sbb

include $(SRCS:.c=.d)
