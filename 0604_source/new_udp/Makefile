# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS =  udp-receive.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    


udp-receive: udp-receive.o
	$(CC) -o $@ $^ $(LIBS) -lpthread

all: $(OBJS)  udp-receive

clean:
	rm -f *.o $(OBJS)  udp-receive

.PHONY: all clean

