
SRCS = page.c sbrk.c fault.c
OBJS = $(SRCS:.c=.o)
OUT = mm.o

all : $(OUT)

TOPDIR = ../..
include $(TOPDIR)/Makedefs

CFLAGS += -DKERNEL

$(OUT) : $(OBJS)
	$(XLD) $(LDFLAGS) -r $(OBJS) -o $(OUT)

clean :
	rm -f *~ $(OBJS) $(OUT)

include $(SRCS:.c=.d)
