
SRCS = malloc.c calloc.c free.c morecore.c realloc.c valloc.c
OBJS = $(SRCS:.c=.o)
OUT = alloc.o

all : $(OUT)

TOPDIR = ../..
include $(TOPDIR)/Makedefs

$(OUT) : $(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o $(OUT)

clean :
	rm -f *~ $(OBJS) $(OUT)

include $(SRCS:.c=.d)
