# Default target.
all:

include ../Makefile.inc

SRCS = net_core.c

LIBA = libnet.a

OBJS = $(SRCS:%.c=%.o)

%.o: %.c
	$(CC) $(CFLAGS) -MD -MP -fpic -c -o $@ $<

$(LIBA): $(OBJS)
	rm -f $@
	$(AR) cq $@ $^
	cp -f $(LIBA) ../../lib/$(ARCH)

all: $(LIBA)

clean:
	rm -f *.o *.d $(LIBA)

.PHONY: all clean
