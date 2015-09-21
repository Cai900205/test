#Default target.
all:

CC = gcc
OPT = -O3
LIBS = -L./lib  -lpthread
LIBS += -L./lib/t4240qds_64b -lusdpaa_srio -lusdpaa_dma -lusdpaa_dma_mem -lusdpaa_of -lusdpaa_process -lm
INCS = -I./include/t4240qds_64b
CFLAGS = -std=gnu99 -Wall -g $(OPT) $(INCS)

SRCS = main.c fvl_srio.c
EXEC = main

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean $(EXEC)
	@cp -f $(EXEC)  ../../bin

clean:
	rm -f *.o *.d $(EXEC)

.PHONY: all clean
