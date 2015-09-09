# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = sw_read.c util.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

sw_read: sw_read.o util.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) sw_read

clean:
	rm -f *.o $(OBJS) sw_read

.PHONY: all clean

