
SRCS = main.c 
OBJS = $(SRCS:.c=.o)

all : init.o startup.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

init.o : $(OBJS)
	$(LD) $(LDFLAGS) -r -o init.o $(OBJS)

clean :
	rm -f *~ *.[od]

include $(SRCS:.c=.d)

