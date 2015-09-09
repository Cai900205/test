# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = cpu_stress.c util.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

cpu_stress: cpu_stress.o util.o
	$(CC) -o $@ $^ $(LIBS) -l pthread

all: $(OBJS) cpu_stress

clean:
	rm -f *.o $(OBJS) cpu_stress

.PHONY: all clean

