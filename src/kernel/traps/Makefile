SRCS = intrap.c debug.c entry.S
OBJS = intrap.o debug.o entry.o

all : traps.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

traps.o : $(OBJS)
	$(XLD) -r -o traps.o $(OBJS)

clean :
	rm -f *~ *.[od]

include intrap.d debug.d

