# Default target.
all:

CC = gcc #$(TILERA_ROOT)/bin/tile-cc

OPT = -Os
CFLAGS = -std=gnu99 -Wall -g $(OPT) 
#LDFLAGS = -static   
LIBS =   -L../lib -lvga_pcie

SRCS = read_reg.c read_loop.c 

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

read_reg: read_reg.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

read_loop: read_loop.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)


all: read_reg read_loop 
	@cp read_reg  ../build
	@cp read_loop ../build
clean:
	rm -f *.o *.d read_reg read_loop 

.PHONY: all clean
