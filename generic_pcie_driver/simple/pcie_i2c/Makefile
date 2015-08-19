# Default target.
all:

ifndef TILERA_ROOT
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = $(TILERA_ROOT)/bin/tile-cc

OPT = -Os
CFLAGS = -std=gnu99 -Wall -g $(OPT) 
LDFLAGS = -static   
LIBS = -ltmc -lgxio  -L../../lib -lsirius_pcie

SRCS = config_pcie.c pcie_i2c.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

config_pcie:$(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: config_pcie
	@cp config_pcie ../../build
clean:
	rm -f *.o *.d config_pcie

.PHONY: all clean
