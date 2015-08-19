# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = rom_write.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

rom_write: rom_write.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) rom_write

clean:
	rm -f *.o $(OBJS) rom_write

.PHONY: all clean

