#Default target.
all:

include ../../Makefile.inc
OPT = -O0
LIBS = -L../../../lib/$(ARCH) -ldfv -lzlog -lpthread
INCS = -I../../../include
CFLAGS += -std=gnu99 -Wall -g $(OPT) $(INCS)

SRCS = main.c
EXEC = dfvtool

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean $(EXEC)
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d $(EXEC)

.PHONY: all clean
