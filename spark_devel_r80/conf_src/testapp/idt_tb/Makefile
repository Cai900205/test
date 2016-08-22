#Default target.
all:

include ../../Makefile.inc
LIB_ROOT = ../../../lib/$(ARCH)
LIBS = -L$(LIB_ROOT) -lidt -lhistory -lreadline -ltermcap -lzlog -lpthread
INCS = -I../../../include
CFLAGS +=  $(INCS)

SRCS = idt_reg.c idt_rtbl.c idt_port.c idt_cli.c idt_tool.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

idt_tool: $(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

all: clean idt_tool
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d idt_tool

.PHONY: all clean
