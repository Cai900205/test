# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = rom_read.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

rom_read: rom_read.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) rom_read

clean:
	rm -f *.o $(OBJS) rom_read

.PHONY: all clean

