#Default target.
all:

include ../Makefile.inc

ifeq ($(ARCH),ppc64)
LIBS += -lidt -lips  -L$(LIB_ROOT)/usdpaa -lusdpaa_srio -lusdpaa_dma -lusdpaa_dma_mem -lusdpaa_of -lusdpaa_process
endif
LIBS += -lcmi -ldfv -lm

SRCS = syslk.c syslk_job.c syslk_cmd.c
EXEC = syslk

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)


all: clean $(EXEC)
#	@cp -f $(EXEC)  ../../bin

clean:
	rm -f *.o *.d $(EXEC)

.PHONY: all clean
