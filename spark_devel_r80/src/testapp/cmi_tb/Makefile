#Default target.
all:

include ../../Makefile.inc
OPT = -O3
LIBS = -L../../../lib/$(ARCH) -lcmi -lzlog -lpthread
INCS = -I../../../include
CFLAGS = -std=gnu99 -Wall -g $(OPT) $(INCS)

SRCS = server.c client.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

server: server.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

client: client.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean server client
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d server client

.PHONY: all clean
