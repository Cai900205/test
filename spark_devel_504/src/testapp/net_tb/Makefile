#Default target.
all:

include ../../Makefile.inc
OPT = -O3
LIBS = -L../../../lib/$(ARCH) -lnet -lzlog -lpthread
INCS = -I../../../include
CFLAGS = -std=gnu99 -Wall -g $(OPT) $(INCS)

SRCS = client.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

client: client.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean client
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d client

.PHONY: all clean
