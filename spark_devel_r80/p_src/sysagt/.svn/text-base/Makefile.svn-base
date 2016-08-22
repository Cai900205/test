#Default target.
all:

include ../Makefile.inc

ifeq ($(ARCH),ppc64)
LIBS += -lidt -lips
endif
LIBS += -lcmi -ldfv -lm

SRCS = sysagt.c
EXEC = sysagt

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)


all: clean $(EXEC)
#	@cp -f $(EXEC)  ../../bin

clean:
	rm -f *.o *.d $(EXEC)

.PHONY: all clean
