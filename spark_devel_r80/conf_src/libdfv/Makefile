# Default target.
all:

include ../Makefile.inc

SRCS = dfv.c dfv_meta.c dfv_repo.c dfv_file.c dfv_cm.c

LIBA = libdfv.a

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
