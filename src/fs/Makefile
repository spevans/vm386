# Makefile for the file system.

SRCS = bitmap.c buffer.c dev.c dir.c file.c fs_cmds.c fs_mod.c inode.c \
       lib.c mkfs.c 
OBJS = $(SRCS:.c=.o)

all : fs.module

TOPDIR = ..
include $(TOPDIR)/Makedefs

CFLAGS += -DFS_MODULE

fs.module : $(OBJS)

clean :
	rm -f *~ *.[od] fs.module *.map bootsect.h

include $(SRCS:.c=.d)
