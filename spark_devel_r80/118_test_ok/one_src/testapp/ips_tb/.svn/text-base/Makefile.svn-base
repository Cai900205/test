#Default target.
all:

include ../../Makefile.inc
LIB_ROOT = ../../../lib/$(ARCH)
LIBS = -L$(LIB_ROOT)  -lzlog -lpthread -lm -liniparser
LIBS += -lidt -ldfv -lips  -L$(LIB_ROOT)/usdpaa -lusdpaa_srio -lusdpaa_dma -lusdpaa_dma_mem -lusdpaa_of -lusdpaa_process
INCS = -I../../../include
CFLAGS +=  $(INCS)

SRCS = loopback.c dual_slave.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

loopback: loopback.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

dual_slave: dual_slave.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean loopback dual_slave
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d loopback dual_slave

.PHONY: all clean
