# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = rtc.c util.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

rtc: rtc.o util.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) rtc

clean:
	rm -f *.o $(OBJS) rtc

.PHONY: all clean

