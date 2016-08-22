# Default target.
all:

include ../Makefile.inc
SRCS = idt_core.c

LIBA = libidt.a

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
