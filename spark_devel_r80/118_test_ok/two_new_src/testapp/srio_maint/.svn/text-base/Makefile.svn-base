#Default target.
all:

include ../../Makefile.inc
LIB_ROOT = ../../../lib/$(ARCH)
LIBS = -L$(LIB_ROOT)  -lzlog -lpthread -lm
LIBS += -lhistory -lreadline -ltermcap -L$(LIB_ROOT)/usdpaa -lusdpaa_srio -lusdpaa_dma -lusdpaa_dma_mem -lusdpaa_of -lusdpaa_process
INCS = -I../../../include -I../../../include/readline -I../../libips/include
CFLAGS +=  $(INCS)

SRCS = srio_maint.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

srio_maint: srio_maint.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean srio_maint
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d srio_maint

.PHONY: all clean
