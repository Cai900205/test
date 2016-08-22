# Default target.
all:

include ./Makefile.inc
SRCS := bman_high.c bman_driver.c \
		qman_high.c qman_utility.c qman_driver.c \
		dpa_alloc.c dpa_sys.c

LIBA = libusdpaa_qbman.a

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
