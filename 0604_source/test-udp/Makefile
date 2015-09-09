# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = udp-send.c udp-receive.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

udp-send: udp-send.o
	$(CC) -o $@ $^ $(LIBS) -lpthread

udp-receive: udp-receive.o
	$(CC) -o $@ $^ $(LIBS) -lpthread

all: $(OBJS) udp-send udp-receive

clean:
	rm -f *.o $(OBJS) udp-send udp-receive

.PHONY: all clean

