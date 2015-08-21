# Default target.
all:

CC = gcc #$(TILERA_ROOT)/bin/tile-cc

OPT = -Os
CFLAGS = -std=gnu99 -Wall -g $(OPT) 
#LDFLAGS = -static   
LIBS =   -L../lib -lvga_pcie

SRCS = read_reg.c read.c 

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

read_reg: read_reg.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

read: read.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)


all: read_reg read 
	@cp read_reg  ../build
	@cp read ../build
clean:
	rm -f *.o *.d read_reg read 

.PHONY: all clean
