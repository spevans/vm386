# Makefile for directory `modules'

SRCS = module.c load.c
OBJS = $(SRCS:.c=.o)

all : mld mstrip mdump modules.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

STATIC_MODS := $(foreach mod, $(shell cat $(TOPDIR)/STATIC), $(TOPDIR)/$(mod).o)

example.module : example_mod.o

modules.o : $(OBJS) __first.o $(STATIC_MODS) __last.o $(TOPDIR)/STATIC
	$(LD) -r -o modules.o $(filter %.o, $^)

clean :
	rm -f *~ *.[od] example.module mld mstrip objlist mdump core

include $(SRCS:.c=.d) mld.d mstrip.d mdump.d
