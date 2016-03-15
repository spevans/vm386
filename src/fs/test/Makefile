
# This is an attempt at building a test filesystem thing in a separate
# directory.

SRCS = bitmap.c buffer.c dev.c dir.c file.c fs_cmds.c fs_mod.c inode.c \
       lib.c mkfs.c test_dev.c \
       shell.c command.c cmds.c test.c \
       printf.c time.c errno.c

vpath %.c .:..:../../shell:../../kernel/misc

OBJS = $(SRCS:.c=.o)

all : fs mktestdev

TOPDIR = ../..
TEST = YES
include $(TOPDIR)/Makedefs

# Compiling for local execution
XCC = $(CC)

CFLAGS += -g -DTEST -DTEST_FS -DTEST_SHELL -DFS_MODULE -DSHELL_MODULE -DKERNEL -Wno-unused-parameter

fs : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

mktestdev : mktestdev.o

clean :
	rm -f *.[od] *~ fs mktestdev wbb test_dev.image

include $(SRCS:.c=.d)
