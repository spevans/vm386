# Makefile for the tools dir

SRCS = e2b.c disasm.c bbin.c bbin16.c makeimage.c bsc.c sysdisk.c btoa.c sbb.c
TOOLS := e2b bbin bbin16 makeimage disasm bsc sysdisk btoa sbb
all : $(TOOLS)

TOPDIR = ..
include $(TOPDIR)/Makedefs

clean :
	rm -f *~ *.[od] $(TOOLS)

include $(SRCS:.c=.d)
