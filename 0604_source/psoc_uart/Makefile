# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = psoc_tmp.c psoc_uart.c util.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

psoc_tmp: psoc_tmp.o psoc_uart.o util.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) psoc_tmp

clean:
	rm -f *.o $(OBJS) psoc_tmp

.PHONY: all clean

