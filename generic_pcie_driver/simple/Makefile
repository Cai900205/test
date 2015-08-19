# Default target.
all:

ifndef TILERA_ROOT
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = $(TILERA_ROOT)/bin/tile-cc

OPT = -Os
CFLAGS = -std=gnu99 -Wall -g $(OPT) 
LDFLAGS = -static   
LIBS = -ltmc -lgxio  -L../lib -lsirius_pcie

SRCS = read_reg.c read_loop.c fpga_mem_test.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

read_reg: read_reg.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

read_loop: read_loop.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

fpga_mem_test:fpga_mem_test.o 
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: read_reg read_loop fpga_mem_test
	@cp read_reg  ../build
	@cp read_loop ../build
	@cp fpga_mem_test ../build
clean:
	rm -f *.o *.d read_reg read_loop fpga_mem_test

.PHONY: all clean
