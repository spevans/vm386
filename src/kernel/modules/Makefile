# Makefile for directory `modules'

SRCS = module.c load.c
OBJS = $(SRCS:.c=.o)

all : modules.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

STATIC_MODS := $(foreach mod, $(shell grep -v ^\# $(TOPDIR)/STATIC), $(TOPDIR)/$(mod).module.o)

example.module : example_mod.o

static_modules.h : $(TOPDIR)/STATIC
	./static_modules.pl <$^ >$@

module.c : static_modules.h

modules.o : $(OBJS) $(STATIC_MODS)
	$(LD) -r -o $@ $(filter %.o, $^)

clean :
	rm -f *~ *.[od] example.module objlist core static_modules.h

include $(SRCS:.c=.d)
