
C_SRCS = task.c sched.c example_tasks.c 
A_SRCS =
OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all : tasks.o 

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

tasks.o : $(OBJS)
	$(XLD) -r -o tasks.o $(OBJS)

clean :
	rm -f *~ *.[od]

include $(C_SRCS:.c=.d)

