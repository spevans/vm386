
SRCS = shell.c command.c cmds.c
OBJS = $(SRCS:.c=.o)

all : shell.module shell

TOPDIR = ..
include $(TOPDIR)/Makedefs

CFLAGS += -DSHELL_MODULE

shell.module : $(OBJS)

SHELL_SRCS = test.c $(SRCS) ../fs/*.c ../fs/test/test_dev.c \
	../kernel/misc/printf.c ../kernel/misc/time.c ../kernel/misc/errno.c

shell: $(SHELL_SRCS)
	gcc -I.. -ggdb -m32 -march=i386 -fno-builtin -Wall -Wstrict-prototypes -fno-strict-aliasing -Werror -Wextra -Wno-attributes -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers -fomit-frame-pointer -DTEST_SHELL  -DSHELL_MODULE -DTEST_FS -DFS_MODULE -DTEST -o $@ $(SHELL_SRCS)

clean :
	rm -f *~ *.[od] shell.module core *.map shell

include $(SRCS:.c=.d)
