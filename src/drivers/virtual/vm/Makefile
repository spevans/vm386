
C_SRCS = fault.c test.c vmach.c vm_mod.c glue.c inslen.c
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : vm.module

TOPDIR = ../../..
include $(TOPDIR)/Makedefs

CFLAGS += -DVM_MODULE

vm.module : $(OBJS)

clean :
	rm -f *~ *.[ods] *.module

include $(C_SRCS:.c=.d)

