# Default target.
all:

include ./Makefile.inc
SRCS := helper_common.c helper_interface.c helper_api.c \
	helper_sec.c helper_pm.c

LIBA = libusdpaa_helper.a

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
