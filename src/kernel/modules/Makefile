# Makefile for directory `modules'

SRCS = module.c load.c
OBJS = $(SRCS:.c=.o)

all : mld mstrip mdump modules.o

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

STATIC_MODS := $(foreach mod, $(shell cat $(TOPDIR)/STATIC), $(TOPDIR)/$(mod).module.o)

example.module : example_mod.o

linker.script : $(TOPDIR)/STATIC
	./build_script.pl <$^ >$@

modules.o : linker.script  $(OBJS) $(STATIC_MODS) $(TOPDIR)/STATIC
	ld -r -T linker.script -Map=modules.map

clean :
	rm -f *~ *.[od] example.module mld mstrip objlist mdump core

include $(SRCS:.c=.d) mld.d mstrip.d mdump.d
