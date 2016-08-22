# Default target.
all:

include ./Makefile.inc
SRCS := srio_driver.c \

LIBA = libusdpaa_srio.a

OBJS = $(SRCS:%.c=%.o)

%.o: %.c
	$(CC) $(CFLAGS) -MD -MP -fpic -c -o $@ $<

$(LIBA): $(OBJS)
	rm -f $@
	$(AR) cq $@ $^
	cp -f $(LIBA) ../../lib/

all: $(LIBA)

clean:
	rm -f *.o *.d $(LIBA)

.PHONY: all clean
